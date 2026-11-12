#include "ui/programwindow.h"

#include "core/editor.h"
#include "render/sliderenderer.h"
#include "ui/icons.h"
#include "ui/theme.h"

#include <QCloseEvent>
#include <QEasingCurve>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QPainter>
#include <QScreen>
#include <QTimer>
#include <QVariantAnimation>
#include <QWindow>

#include <algorithm>

namespace mxu::ui {

ProgramWindow::ProgramWindow(Editor *editor, SlideRenderer *renderer, QWidget *parent)
    : QWidget(parent)
    , m_editor(editor)
    , m_renderer(renderer)
{
    setObjectName(QStringLiteral("ProgramOutput"));
    setWindowTitle(QStringLiteral("MxU Slides — Program Output"));
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setWindowFlags(Qt::Window | Qt::WindowDoesNotAcceptFocus);
    setStyleSheet(QStringLiteral("background: black;"));

    m_fadeAnimation = new QVariantAnimation(this);
    m_fadeAnimation->setEasingCurve(QEasingCurve::InOutQuad);
    connect(m_fadeAnimation, &QVariantAnimation::valueChanged, this,
            [this](const QVariant &value) {
                m_fade = value.toReal();
                update();
            });
    connect(m_fadeAnimation, &QVariantAnimation::finished, this, [this] {
        m_fade = 1.0;
        m_previous = QImage();
        update();
    });

    m_clock = new QTimer(this);
    m_clock->setInterval(500);
    connect(m_clock, &QTimer::timeout, this, [this] {
        if (m_presenting)
            refresh();
    });

    connect(editor, &Editor::liveChanged, this, &ProgramWindow::refresh);
    connect(editor, &Editor::presentationEdited, this, &ProgramWindow::refresh);
    connect(editor, &Editor::presentationChanged, this, &ProgramWindow::refresh);
    connect(editor, &Editor::timerChanged, this, &ProgramWindow::refresh);
}

ProgramWindow::~ProgramWindow() = default;

void ProgramWindow::setTargetScreen(QScreen *screen)
{
    if (m_screen == screen)
        return;
    m_screen = screen;
    if (m_presenting) {
        configureWindow();
        refresh();
    }
}

void ProgramWindow::configureWindow()
{
    Qt::WindowFlags flags = Qt::Window | Qt::WindowDoesNotAcceptFocus;
    if (m_screen)
        flags |= Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint;
    setWindowFlags(flags);
    setAttribute(Qt::WA_ShowWithoutActivating, true);

    if (m_screen) {
        if (windowHandle())
            windowHandle()->setScreen(m_screen);
        setCursor(Qt::BlankCursor);
        setGeometry(m_screen->geometry());
        showFullScreen();
    } else {
        setCursor(Qt::ArrowCursor);
        if (size().isEmpty() || isFullScreen())
            resize(1120, 630);
        showNormal();
        show();
    }
    raise();
}

void ProgramWindow::setPresenting(bool presenting)
{
    if (m_presenting == presenting) {
        if (presenting)
            refresh();
        return;
    }
    m_presenting = presenting;
    if (!presenting) {
        m_fadeAnimation->stop();
        m_clock->stop();
        hide();
        m_current = QImage();
        m_previous = QImage();
        m_lastIndex = -1;
        m_fade = 1.0;
        return;
    }

    m_lastIndex = -1;
    configureWindow();
    m_clock->start();
    refresh();
    setWindowTitle(QStringLiteral("MxU Slides — Program Output"));
}

QImage ProgramWindow::renderSlideImage(int index) const
{
    if (!m_editor->hasPresentation() || index < 0
        || index >= m_editor->presentation().slides.size()) {
        return {};
    }
    const Presentation &deck = m_editor->presentation();
    RenderContext context;
    context.clock = QTime::currentTime();
    context.timerSeconds = m_editor->timerRemainingSeconds();
    context.slideIndex = index;
    context.slideCount = deck.slides.size();
    context.presentationName = deck.name;
    return m_renderer->render(deck, deck.slides.at(index), m_editor->theme(),
                              QSize(deck.canvasWidth, deck.canvasHeight), context);
}

void ProgramWindow::refresh()
{
    if (!m_presenting)
        return;
    if (!m_editor->hasPresentation() || m_editor->presentation().slides.isEmpty()) {
        m_current = QImage();
        m_previous = QImage();
        update();
        return;
    }

    const Presentation &deck = m_editor->presentation();
    int index = m_editor->liveIndex() >= 0 ? m_editor->liveIndex() : m_editor->currentIndex();
    index = qBound(0, index, int(deck.slides.size()) - 1);
    const bool blackout = m_editor->blackout();
    const bool logo = m_editor->logoVisible();

    if (blackout || logo) {
        m_fadeAnimation->stop();
        m_previous = QImage();
        m_fade = 1.0;
        m_current = QImage();
    } else {
        const QImage frame = renderSlideImage(index);
        const bool changed = index != m_lastIndex;
        if (changed && m_lastIndex >= 0 && !m_current.isNull()) {
            m_previous = m_current;
            m_fade = 0.0;
            const Transition &transition =
                deck.slides.at(index).transition.enabled ? deck.slides.at(index).transition
                                                         : deck.transition;
            const int durationMs =
                transition.enabled && transition.kind != QLatin1String("cut")
                ? qRound(std::clamp(transition.durationSeconds, 0.0, 3.0) * 1000.0)
                : 0;
            if (durationMs > 30) {
                m_fadeAnimation->stop();
                m_fadeAnimation->setDuration(durationMs);
                m_fadeAnimation->setStartValue(0.0);
                m_fadeAnimation->setEndValue(1.0);
                m_fadeAnimation->start();
            } else {
                m_fade = 1.0;
                m_previous = QImage();
            }
        } else if (changed) {
            m_previous = QImage();
            m_fade = 1.0;
        }
        m_current = frame;
        setWindowTitle(QStringLiteral("MxU Slides — %1 — Program Output").arg(deck.name));
    }

    m_lastIndex = index;
    update();
}

void ProgramWindow::paintFrame(QPainter &painter, const QImage &image, qreal opacity) const
{
    if (image.isNull() || opacity <= 0.0)
        return;
    painter.save();
    painter.setOpacity(opacity);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    const QSizeF scaled = image.size().scaled(size(), Qt::KeepAspectRatio);
    const QRectF target((width() - scaled.width()) / 2.0,
                        (height() - scaled.height()) / 2.0, scaled.width(), scaled.height());
    painter.drawImage(target, image);
    painter.restore();
}

void ProgramWindow::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);
    if (!m_presenting)
        return;

    if (m_editor->blackout())
        return;

    if (m_editor->logoVisible()) {
        const Theme theme = m_editor->theme();
        painter.fillRect(rect(), theme.backgroundColor());
        const int side = int(std::min(width(), height()) * 0.30);
        paintAppMark(painter, QRectF(width() / 2.0 - side / 2.0,
                                     height() / 2.0 - side / 2.0 - height() * 0.03, side, side));
        QFont font = uiFont(10, QFont::DemiBold);
        font.setPixelSize(qMax(14, height() / 26));
        painter.setFont(font);
        painter.setPen(theme.textColor());
        painter.drawText(QRectF(0, height() / 2.0 + side / 2.0, width(), height() * 0.08),
                         Qt::AlignCenter, QStringLiteral("MxU Slides"));
        return;
    }

    if (m_current.isNull()) {
        painter.setPen(QColor(110, 118, 130));
        painter.setFont(uiFont(11));
        painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("Waiting for presentation…"));
        return;
    }

    if (!m_previous.isNull() && m_fade < 1.0) {
        paintFrame(painter, m_previous, 1.0);
        paintFrame(painter, m_current, m_fade);
    } else {
        paintFrame(painter, m_current, 1.0);
    }
}

void ProgramWindow::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Escape:
        emit stopRequested();
        event->accept();
        return;
    case Qt::Key_B:
        m_editor->setBlackout(!m_editor->blackout());
        event->accept();
        return;
    case Qt::Key_L:
        m_editor->setLogoVisible(!m_editor->logoVisible());
        event->accept();
        return;
    case Qt::Key_Space:
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Right:
    case Qt::Key_PageDown:
        m_editor->next();
        event->accept();
        return;
    case Qt::Key_Left:
    case Qt::Key_PageUp:
        m_editor->previous();
        event->accept();
        return;
    case Qt::Key_Home:
        m_editor->goToSlideNumber(1);
        event->accept();
        return;
    case Qt::Key_End:
        m_editor->goToSlideNumber(m_editor->presentation().slides.size());
        event->accept();
        return;
    default:
        break;
    }
    QWidget::keyPressEvent(event);
}

void ProgramWindow::closeEvent(QCloseEvent *event)
{
    if (m_presenting)
        emit stopRequested();
    hide();
    event->accept();
}

} // namespace mxu::ui
