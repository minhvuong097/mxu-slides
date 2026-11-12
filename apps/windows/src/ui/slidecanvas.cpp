#include "ui/slidecanvas.h"

#include "core/editor.h"
#include "core/library.h"
#include "render/sliderenderer.h"
#include "ui/icons.h"
#include "ui/theme.h"
#include "ui/widgets.h"

#include <QContextMenuEvent>
#include <QCursor>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QTransform>
#include <QWheelEvent>

#include <algorithm>

namespace mxu::ui {
namespace {

QRectF objectFrame(const SlideObject &object, const Presentation &presentation)
{
    if (object.hasFrame && object.frame.width() > 0.0 && object.frame.height() > 0.0)
        return object.frame;
    return QRectF(presentation.canvasWidth * 0.06, presentation.canvasHeight * 0.16,
                  presentation.canvasWidth * 0.88, presentation.canvasHeight * 0.66);
}

QPointF rotatedPoint(const QPointF &point, const QPointF &center, double degrees)
{
    if (qFuzzyIsNull(degrees))
        return point;
    QTransform transform;
    transform.translate(center.x(), center.y());
    transform.rotate(degrees);
    transform.translate(-center.x(), -center.y());
    return transform.map(point);
}

} // namespace

SlideCanvas::SlideCanvas(Editor *editor, SlideRenderer *renderer, QWidget *parent)
    : QWidget(parent)
    , m_editor(editor)
    , m_renderer(renderer)
{
    setObjectName(QStringLiteral("CanvasHost"));
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setMinimumSize(320, 220);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    connect(editor, &Editor::presentationChanged, this, &SlideCanvas::refresh);
    connect(editor, &Editor::presentationEdited, this, &SlideCanvas::refresh);
    connect(editor, &Editor::slideIndexChanged, this, &SlideCanvas::refresh);
    connect(editor, &Editor::selectionChanged, this, &SlideCanvas::refresh);
}

void SlideCanvas::refresh()
{
    updateHover(mapFromGlobal(QCursor::pos()));
    update();
}

void SlideCanvas::fitToWindow()
{
    m_zoom = 1.0;
    m_panOffset = QPointF();
    update();
    emitZoom();
}

void SlideCanvas::zoomIn()
{
    m_zoom = std::min(2.5, m_zoom * 1.15);
    update();
    emitZoom();
}

void SlideCanvas::zoomOut()
{
    m_zoom = std::max(0.4, m_zoom / 1.15);
    update();
    emitZoom();
}

int SlideCanvas::zoomPercent() const
{
    const qreal scale = m_scale > 0.0 ? m_scale : fitScale();
    return qRound(scale * 100.0);
}

qreal SlideCanvas::fitScale() const
{
    if (!m_editor->hasPresentation())
        return 1.0;
    const QRectF available = QRectF(rect()).adjusted(28, 28, -28, -28);
    const Presentation &deck = m_editor->presentation();
    if (deck.canvasWidth <= 0 || deck.canvasHeight <= 0 || available.width() <= 0.0
        || available.height() <= 0.0) {
        return 1.0;
    }
    return std::min(available.width() / deck.canvasWidth,
                    available.height() / deck.canvasHeight);
}

QRectF SlideCanvas::computeSlideRect() const
{
    if (!m_editor->hasPresentation())
        return {};
    const QRectF available = QRectF(rect()).adjusted(28, 28, -28, -28);
    const Presentation &deck = m_editor->presentation();
    const qreal scale = fitScale() * m_zoom;
    const QSizeF size(deck.canvasWidth * scale, deck.canvasHeight * scale);
    const QPointF center = available.center() + m_panOffset;
    return QRectF(center.x() - size.width() / 2.0, center.y() - size.height() / 2.0,
                  size.width(), size.height());
}

QPointF SlideCanvas::toSlide(const QPointF &point) const
{
    if (m_scale <= 0.0)
        return {};
    return (point - m_slideRect.topLeft()) / m_scale;
}

QPointF SlideCanvas::toView(const QPointF &point) const
{
    return m_slideRect.topLeft() + point * m_scale;
}

QRectF SlideCanvas::toView(const QRectF &rect) const
{
    return QRectF(toView(rect.topLeft()), rect.size() * m_scale);
}

void SlideCanvas::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const Palette &p = mxu::ui::palette();
    painter.fillRect(rect(), QColor(14, 15, 18));

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 255, 255, 8));
    for (int y = 12; y < height(); y += 24) {
        for (int x = 12; x < width(); x += 24)
            painter.drawEllipse(QPointF(x, y), 0.9, 0.9);
    }

    if (!m_editor->hasPresentation() || m_editor->presentation().slides.isEmpty()) {
        drawEmptyState(painter);
        return;
    }

    m_slideRect = computeSlideRect();
    m_scale = m_slideRect.width() / m_editor->presentation().canvasWidth;

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 55));
    painter.drawRoundedRect(m_slideRect.adjusted(-5, 3, 5, 14), 11, 11);
    painter.setBrush(QColor(0, 0, 0, 70));
    painter.drawRoundedRect(m_slideRect.adjusted(-2, 1, 2, 8), 9, 9);

    QPainterPath clip;
    clip.addRoundedRect(m_slideRect, 7, 7);
    painter.save();
    painter.setClipPath(clip);
    painter.fillRect(m_slideRect, Qt::black);

    const Slide *slide = m_editor->currentSlide();
    if (slide) {
        RenderContext context;
        context.slideIndex = m_editor->currentIndex();
        context.slideCount = m_editor->presentation().slides.size();
        context.presentationName = m_editor->presentation().name;
        context.timerSeconds = m_editor->timerRemainingSeconds();
        m_renderer->paint(painter, m_editor->presentation(), *slide, m_editor->theme(),
                          m_slideRect, context);
    }
    painter.restore();

    painter.setPen(QPen(QColor(255, 255, 255, 28), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(clip);

    if (const SlideObject *selected = m_editor->selectedObject()) {
        const QRectF view = selectedViewRect();
        painter.save();
        if (!qFuzzyIsNull(selected->rotationDegrees)) {
            QTransform transform;
            transform.translate(view.center().x(), view.center().y());
            transform.rotate(selected->rotationDegrees);
            transform.translate(-view.center().x(), -view.center().y());
            painter.setTransform(transform, true);
        }
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(p.accent, 1.6));
        painter.drawRect(view);
        painter.restore();

        painter.setPen(QPen(p.accent, 1.4));
        painter.setBrush(Qt::white);
        for (Handle handle : {Handle::TopLeft, Handle::Top, Handle::TopRight, Handle::Right,
                              Handle::BottomRight, Handle::Bottom, Handle::BottomLeft,
                              Handle::Left}) {
            painter.drawRoundedRect(handleRect(handle), 2, 2);
        }

        if (!selected->name.isEmpty()) {
            QFont labelFont = uiFont(8, QFont::DemiBold);
            painter.setFont(labelFont);
            const QFontMetrics metrics(labelFont);
            const QString label = metrics.elidedText(selected->name, Qt::ElideRight, 180);
            const QRectF badge(view.left(), view.top() - 21, metrics.horizontalAdvance(label) + 14,
                               19);
            painter.setPen(Qt::NoPen);
            painter.setBrush(p.accent);
            painter.drawRoundedRect(badge, 5, 5);
            painter.setPen(Qt::white);
            painter.drawText(badge, Qt::AlignCenter, label);
        }
    } else if (!m_hoverId.isEmpty()) {
        if (const Slide *current = m_editor->currentSlide()) {
            const int index = current->indexOfObject(m_hoverId);
            if (index >= 0) {
                QPen hover(p.accent, 1.1, Qt::DashLine);
                painter.setPen(hover);
                painter.setBrush(Qt::NoBrush);
                painter.drawRect(toView(objectFrame(current->objects.at(index),
                                                    m_editor->presentation())));
            }
        }
    }

    emitZoom();
}

void SlideCanvas::drawEmptyState(QPainter &painter)
{
    const Palette &p = mxu::ui::palette();
    const QPointF center(width() / 2.0, height() / 2.0 - 20);
    paintAppMark(painter, QRectF(center.x() - 30, center.y() - 60, 60, 60));
    painter.setPen(p.text);
    painter.setFont(uiFont(13, QFont::DemiBold));
    painter.drawText(QRectF(0, center.y() + 14, width(), 24), Qt::AlignCenter,
                     QStringLiteral("No presentation open"));
    painter.setPen(p.textMuted);
    painter.setFont(uiFont(9));
    painter.drawText(QRectF(0, center.y() + 40, width(), 20), Qt::AlignCenter,
                     QStringLiteral("Choose a presentation in the library or create a new one."));
}

QRectF SlideCanvas::selectedViewRect() const
{
    if (const SlideObject *selected = m_editor->selectedObject())
        return toView(objectFrame(*selected, m_editor->presentation()));
    return {};
}

QString SlideCanvas::objectAt(const QPointF &viewPoint) const
{
    const Slide *slide = m_editor->currentSlide();
    if (!slide || !m_slideRect.contains(viewPoint))
        return {};
    const QPointF slidePoint = toSlide(viewPoint);
    for (int index = slide->objects.size() - 1; index >= 0; --index) {
        const SlideObject &object = slide->objects.at(index);
        if (object.hidden)
            continue;
        QRectF frame = objectFrame(object, m_editor->presentation());
        const QPointF local = rotatedPoint(slidePoint, frame.center(), -object.rotationDegrees);
        if (frame.contains(local))
            return object.id;
    }
    return {};
}

SlideCanvas::Handle SlideCanvas::handleAt(const QPointF &viewPoint) const
{
    if (m_editor->selectedObjectId().isEmpty())
        return Handle::None;
    const std::pair<Handle, QRectF> candidates[] = {
        {Handle::TopLeft, handleRect(Handle::TopLeft)},
        {Handle::Top, handleRect(Handle::Top)},
        {Handle::TopRight, handleRect(Handle::TopRight)},
        {Handle::Right, handleRect(Handle::Right)},
        {Handle::BottomRight, handleRect(Handle::BottomRight)},
        {Handle::Bottom, handleRect(Handle::Bottom)},
        {Handle::BottomLeft, handleRect(Handle::BottomLeft)},
        {Handle::Left, handleRect(Handle::Left)},
    };
    for (const auto &[handle, box] : candidates) {
        if (box.adjusted(-3, -3, 3, 3).contains(viewPoint))
            return handle;
    }
    return Handle::None;
}

QRectF SlideCanvas::handleRect(Handle handle) const
{
    const QRectF view = selectedViewRect();
    const qreal size = 8.0;
    QPointF center;
    switch (handle) {
    case Handle::TopLeft:
        center = view.topLeft();
        break;
    case Handle::Top:
        center = QPointF(view.center().x(), view.top());
        break;
    case Handle::TopRight:
        center = view.topRight();
        break;
    case Handle::Right:
        center = QPointF(view.right(), view.center().y());
        break;
    case Handle::BottomRight:
        center = view.bottomRight();
        break;
    case Handle::Bottom:
        center = QPointF(view.center().x(), view.bottom());
        break;
    case Handle::BottomLeft:
        center = view.bottomLeft();
        break;
    case Handle::Left:
        center = QPointF(view.left(), view.center().y());
        break;
    case Handle::None:
        return {};
    }
    return QRectF(center.x() - size / 2.0, center.y() - size / 2.0, size, size);
}

void SlideCanvas::updateHover(const QPointF &viewPoint)
{
    if (m_interaction != Interaction::None)
        return;
    const QString previous = m_hoverId;
    m_hoverId = objectAt(viewPoint);
    if (previous != m_hoverId)
        update();
    updateCursor(viewPoint);
}

void SlideCanvas::updateCursor(const QPointF &viewPoint)
{
    const Handle handle = handleAt(viewPoint);
    if (handle != Handle::None) {
        setCursor(cursorForHandle(handle));
        return;
    }
    if (!objectAt(viewPoint).isEmpty()) {
        setCursor(Qt::SizeAllCursor);
        return;
    }
    setCursor(Qt::ArrowCursor);
}

QCursor SlideCanvas::cursorForHandle(Handle handle) const
{
    switch (handle) {
    case Handle::TopLeft:
    case Handle::BottomRight:
        return Qt::SizeFDiagCursor;
    case Handle::TopRight:
    case Handle::BottomLeft:
        return Qt::SizeBDiagCursor;
    case Handle::Top:
    case Handle::Bottom:
        return Qt::SizeVerCursor;
    case Handle::Left:
    case Handle::Right:
        return Qt::SizeHorCursor;
    case Handle::None:
        return Qt::ArrowCursor;
    }
    return Qt::ArrowCursor;
}

void SlideCanvas::mousePressEvent(QMouseEvent *event)
{
    setFocus(Qt::MouseFocusReason);
    const QPointF view = event->position();

    if (event->button() == Qt::MiddleButton) {
        m_interaction = Interaction::Pan;
        m_pressView = view;
        m_panStart = m_panOffset;
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    m_pressView = view;
    m_pressSlide = toSlide(view);
    m_dragMoved = false;

    const Handle handle = handleAt(view);
    if (handle != Handle::None) {
        if (const SlideObject *selected = m_editor->selectedObject()) {
            m_interaction = Interaction::Resize;
            m_activeHandle = handle;
            m_draggedId = selected->id;
            m_pressFrame = objectFrame(*selected, m_editor->presentation());
            m_editor->beginInteractiveEdit();
            event->accept();
            return;
        }
    }

    const QString id = objectAt(view);
    if (id.isEmpty()) {
        m_editor->selectObject(QString());
        m_interaction = Interaction::None;
        m_hoverId.clear();
        update();
        event->accept();
        return;
    }

    m_editor->selectObject(id);
    m_draggedId = id;
    m_interaction = Interaction::Move;
    if (const SlideObject *selected = m_editor->selectedObject()) {
        m_pressFrame = objectFrame(*selected, m_editor->presentation());
        m_editor->beginInteractiveEdit();
    }
    setCursor(Qt::SizeAllCursor);
    event->accept();
}

void SlideCanvas::mouseMoveEvent(QMouseEvent *event)
{
    const QPointF view = event->position();

    if (m_interaction == Interaction::Pan) {
        m_panOffset = m_panStart + (view - m_pressView);
        update();
        event->accept();
        return;
    }
    if (m_interaction == Interaction::None) {
        updateHover(view);
        event->accept();
        return;
    }

    Slide *slide = m_editor->currentSlideMutable();
    if (!slide)
        return;
    const int index = slide->indexOfObject(m_draggedId);
    if (index < 0)
        return;

    const QPointF delta = toSlide(view) - m_pressSlide;
    QRectF frame = m_pressFrame;

    if (m_interaction == Interaction::Move) {
        frame.translate(delta);
        m_dragMoved = true;
    } else {
        const qreal minSize = 24.0;
        if (m_activeHandle == Handle::Left || m_activeHandle == Handle::TopLeft
            || m_activeHandle == Handle::BottomLeft) {
            frame.setLeft(std::min(m_pressFrame.left() + delta.x(), m_pressFrame.right() - minSize));
        }
        if (m_activeHandle == Handle::Right || m_activeHandle == Handle::TopRight
            || m_activeHandle == Handle::BottomRight) {
            frame.setRight(std::max(m_pressFrame.right() + delta.x(), m_pressFrame.left() + minSize));
        }
        if (m_activeHandle == Handle::Top || m_activeHandle == Handle::TopLeft
            || m_activeHandle == Handle::TopRight) {
            frame.setTop(std::min(m_pressFrame.top() + delta.y(), m_pressFrame.bottom() - minSize));
        }
        if (m_activeHandle == Handle::Bottom || m_activeHandle == Handle::BottomLeft
            || m_activeHandle == Handle::BottomRight) {
            frame.setBottom(std::max(m_pressFrame.bottom() + delta.y(),
                                     m_pressFrame.top() + minSize));
        }
        m_dragMoved = true;
    }

    applyFrame(frame, m_interaction == Interaction::Resize, m_activeHandle);
    update();
    event->accept();
}

void SlideCanvas::applyFrame(const QRectF &frame, bool resize, Handle handle)
{
    Q_UNUSED(resize)
    Q_UNUSED(handle)
    Slide *slide = m_editor->currentSlideMutable();
    if (!slide)
        return;
    const int index = slide->indexOfObject(m_draggedId);
    if (index < 0)
        return;
    slide->objects[index].hasFrame = true;
    slide->objects[index].frame = frame.normalized();
}

void SlideCanvas::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton && m_interaction == Interaction::Pan) {
        m_interaction = Interaction::None;
        setCursor(Qt::ArrowCursor);
        event->accept();
        return;
    }
    if (event->button() != Qt::LeftButton) {
        QWidget::mouseReleaseEvent(event);
        return;
    }
    if (m_interaction == Interaction::Move || m_interaction == Interaction::Resize) {
        if (m_dragMoved)
            m_editor->endInteractiveEdit(m_interaction == Interaction::Move
                                             ? QStringLiteral("Move object")
                                             : QStringLiteral("Resize object"));
        else
            m_editor->endInteractiveEdit(QStringLiteral("Edit object"));
    }
    m_interaction = Interaction::None;
    m_activeHandle = Handle::None;
    m_draggedId.clear();
    m_dragMoved = false;
    updateHover(event->position());
    event->accept();
}

void SlideCanvas::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mouseDoubleClickEvent(event);
        return;
    }
    const QString id = objectAt(event->position());
    if (id.isEmpty())
        return;
    const Slide *slide = m_editor->currentSlide();
    const int index = slide ? slide->indexOfObject(id) : -1;
    if (index >= 0 && slide->objects.at(index).isText()) {
        m_editor->selectObject(id);
        emit textEditingRequested(id);
    }
    event->accept();
}

void SlideCanvas::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        if (event->angleDelta().y() > 0)
            zoomIn();
        else if (event->angleDelta().y() < 0)
            zoomOut();
        event->accept();
        return;
    }
    QWidget::wheelEvent(event);
}

void SlideCanvas::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        m_editor->selectObject(QString());
        return;
    }
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        if (!m_editor->selectedObjectId().isEmpty()) {
            m_editor->deleteSelectedObject();
            return;
        }
    }
    if (event->modifiers() == Qt::ControlModifier && event->key() == Qt::Key_D) {
        m_editor->duplicateSelectedObject();
        return;
    }
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (const SlideObject *selected = m_editor->selectedObject()) {
            if (selected->isText()) {
                emit textEditingRequested(selected->id);
                return;
            }
        }
    }

    const double step = (event->modifiers() & Qt::ShiftModifier) ? 10.0 : 2.0;
    switch (event->key()) {
    case Qt::Key_Left:
        nudgeSelected(-step, 0);
        return;
    case Qt::Key_Right:
        nudgeSelected(step, 0);
        return;
    case Qt::Key_Up:
        nudgeSelected(0, -step);
        return;
    case Qt::Key_Down:
        nudgeSelected(0, step);
        return;
    default:
        break;
    }
    QWidget::keyPressEvent(event);
}

void SlideCanvas::nudgeSelected(double dx, double dy)
{
    if (m_editor->selectedObjectId().isEmpty())
        return;
    const QString id = m_editor->selectedObjectId();
    const int slideIndex = m_editor->currentIndex();
    m_editor->commit(QStringLiteral("Nudge object"), [id, dx, dy, slideIndex](Presentation &deck) {
        if (slideIndex < 0 || slideIndex >= deck.slides.size())
            return;
        for (SlideObject &object : deck.slides[slideIndex].objects) {
            if (object.id != id)
                continue;
            object.hasFrame = true;
            object.frame.translate(dx, dy);
            break;
        }
    });
}

void SlideCanvas::contextMenuEvent(QContextMenuEvent *event)
{
    const QPointF view = event->pos();
    if (!objectAt(view).isEmpty())
        m_editor->selectObject(objectAt(view));

    QMenu menu(this);
    QAction *addTextAction = menu.addAction(ui::icon(Glyph::Text, mxu::ui::palette().textMuted, 16),
                                            QStringLiteral("Add text"));
    QAction *addShapeAction = menu.addAction(ui::icon(Glyph::Shape, mxu::ui::palette().textMuted, 16),
                                             QStringLiteral("Add shape"));
    QAction *addImageAction = menu.addAction(ui::icon(Glyph::Image, mxu::ui::palette().textMuted, 16),
                                             QStringLiteral("Add image…"));
    menu.addSeparator();

    QAction *editText = nullptr;
    QAction *duplicate = nullptr;
    QAction *forward = nullptr;
    QAction *backward = nullptr;
    QAction *remove = nullptr;
    if (const SlideObject *selected = m_editor->selectedObject()) {
        if (selected->isText())
            editText = menu.addAction(QStringLiteral("Edit text…"));
        duplicate = menu.addAction(QStringLiteral("Duplicate"));
        forward = menu.addAction(QStringLiteral("Bring forward"));
        backward = menu.addAction(QStringLiteral("Send backward"));
        menu.addSeparator();
        remove = menu.addAction(ui::icon(Glyph::Trash, mxu::ui::palette().live, 16),
                                QStringLiteral("Delete"));
    }

    QAction *chosen = menu.exec(event->globalPos());
    if (!chosen)
        return;
    if (chosen == addTextAction)
        addText();
    else if (chosen == addShapeAction)
        addShape();
    else if (chosen == addImageAction)
        addImage();
    else if (chosen == editText)
        emit textEditingRequested(m_editor->selectedObjectId());
    else if (chosen == duplicate)
        m_editor->duplicateSelectedObject();
    else if (chosen == forward)
        m_editor->bringSelectedForward();
    else if (chosen == backward)
        m_editor->sendSelectedBackward();
    else if (chosen == remove)
        m_editor->deleteSelectedObject();
}

void SlideCanvas::addText()
{
    if (!m_editor->hasPresentation())
        return;
    const Presentation &deck = m_editor->presentation();
    SlideObject object = makeTextObject(
        QStringLiteral("Text"), QStringLiteral("Double-click to edit"),
        QRectF(deck.canvasWidth * 0.15, deck.canvasHeight * 0.35, deck.canvasWidth * 0.70,
               deck.canvasHeight * 0.30),
        72.0);
    object.id = newId();
    m_editor->addObject(object);
    emit textEditingRequested(object.id);
}

void SlideCanvas::addShape()
{
    if (!m_editor->hasPresentation())
        return;
    const Presentation &deck = m_editor->presentation();
    SlideObject object = makeShapeObject(
        QStringLiteral("Shape"), QStringLiteral("roundedRectangle"),
        QRectF(deck.canvasWidth * 0.28, deck.canvasHeight * 0.36, deck.canvasWidth * 0.44,
               deck.canvasHeight * 0.28),
        mxu::ui::palette().accent);
    object.id = newId();
    m_editor->addObject(object);
}

void SlideCanvas::addImage()
{
    if (!m_editor->hasPresentation())
        return;
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Add image"), QString(),
        QStringLiteral("Images (*.png *.jpg *.jpeg *.bmp *.webp *.gif *.tif *.tiff);;All files (*)"));
    if (path.isEmpty())
        return;
    QString error;
    QString warning;
    const QString mediaId = m_editor->importMediaFile(path, &error, &warning);
    if (mediaId.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Add image"), error);
        return;
    }
    if (!warning.isEmpty())
        QMessageBox::information(this, QStringLiteral("Add image"), warning);

    const Presentation &deck = m_editor->presentation();
    const qreal width = deck.canvasWidth * 0.70;
    const qreal height = deck.canvasHeight * 0.70;
    SlideObject object;
    object.id = newId();
    object.objectKind = QStringLiteral("media");
    object.name = QFileInfo(path).completeBaseName();
    object.mediaId = mediaId;
    object.mediaScaleMode = QStringLiteral("fit");
    object.hasFrame = true;
    object.frame = QRectF((deck.canvasWidth - width) / 2.0, (deck.canvasHeight - height) / 2.0,
                          width, height);
    m_editor->addObject(object);
}

void SlideCanvas::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    update();
    emitZoom();
}

void SlideCanvas::emitZoom()
{
    const int percent = zoomPercent();
    if (percent == m_lastZoomPercent)
        return;
    m_lastZoomPercent = percent;
    emit zoomChanged(percent);
}

} // namespace mxu::ui
