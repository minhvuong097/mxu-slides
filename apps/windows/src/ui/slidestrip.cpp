#include "ui/slidestrip.h"

#include "core/editor.h"
#include "render/sliderenderer.h"
#include "ui/icons.h"
#include "ui/theme.h"
#include "ui/widgets.h"

#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QStyledItemDelegate>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

namespace mxu::ui {
namespace {

class SlideDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override
    {
        return QSize(172, 132);
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        const Palette &p = mxu::ui::palette();
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        const QRectF card = option.rect.adjusted(4, 4, -4, -4);
        const bool selected = option.state & QStyle::State_Selected;
        const bool hovered = option.state & QStyle::State_MouseOver;

        QPainterPath cardPath;
        cardPath.addRoundedRect(card, 9, 9);
        painter->fillPath(cardPath, selected ? p.accentSoft : hovered ? p.surface : p.panel);
        if (selected || hovered) {
            painter->setPen(QPen(selected ? p.accent : p.border, selected ? 1.4 : 1.0));
            painter->setBrush(Qt::NoBrush);
            painter->drawPath(cardPath);
        }

        const QRectF thumbRect(card.left() + 7, card.top() + 7, card.width() - 14,
                               (card.width() - 14) * 9.0 / 16.0);
        QPainterPath thumbPath;
        thumbPath.addRoundedRect(thumbRect, 6, 6);
        painter->save();
        painter->setClipPath(thumbPath);
        painter->fillRect(thumbRect, QColor(10, 12, 15));
        const QPixmap preview = index.data(Qt::UserRole).value<QPixmap>();
        if (!preview.isNull()) {
            painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
            painter->drawPixmap(thumbRect, preview, QRectF(preview.rect()));
        } else {
            painter->setPen(p.textFaint);
            painter->drawText(thumbRect, Qt::AlignCenter, QStringLiteral("No preview"));
        }
        painter->restore();
        painter->setPen(QPen(QColor(255, 255, 255, 20), 1));
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(thumbPath);

        const QString number = QString::number(index.row() + 1);
        const QRectF badge(thumbRect.left() + 6, thumbRect.top() + 6, 23, 17);
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(0, 0, 0, 160));
        painter->drawRoundedRect(badge, 5, 5);
        QFont numberFont = option.font;
        numberFont.setPointSizeF(8);
        numberFont.setWeight(QFont::DemiBold);
        painter->setFont(numberFont);
        painter->setPen(Qt::white);
        painter->drawText(badge, Qt::AlignCenter, number);

        if (index.data(Qt::UserRole + 2).toBool()) {
            const QRectF live(thumbRect.right() - 40, thumbRect.top() + 6, 34, 17);
            painter->setPen(Qt::NoPen);
            painter->setBrush(p.live);
            painter->drawRoundedRect(live, 5, 5);
            QFont liveFont = option.font;
            liveFont.setPointSizeF(7);
            liveFont.setWeight(QFont::Bold);
            painter->setFont(liveFont);
            painter->setPen(Qt::white);
            painter->drawText(live, Qt::AlignCenter, QStringLiteral("LIVE"));
        }

        QFont nameFont = option.font;
        nameFont.setPointSizeF(8.8);
        nameFont.setWeight(selected ? QFont::DemiBold : QFont::Normal);
        painter->setFont(nameFont);
        painter->setPen(selected ? p.text : p.textMuted);
        const QString name = index.data(Qt::DisplayRole).toString();
        const QRectF nameRect(card.left() + 8, thumbRect.bottom() + 6, card.width() - 16, 17);
        painter->drawText(nameRect, Qt::AlignLeft | Qt::AlignVCenter,
                          painter->fontMetrics().elidedText(name, Qt::ElideRight,
                                                           int(nameRect.width())));
        painter->restore();
    }
};

} // namespace

SlideStrip::SlideStrip(Editor *editor, SlideRenderer *renderer, QWidget *parent)
    : QFrame(parent)
    , m_editor(editor)
    , m_renderer(renderer)
{
    setFixedHeight(198);
    setObjectName(QStringLiteral("SlideStrip"));
    setStyleSheet(QStringLiteral("QFrame#SlideStrip { background: %1; border-top: 1px solid %2; }")
                      .arg(mxu::ui::palette().panel.name(), mxu::ui::palette().border.name()));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(6);

    auto *header = new QHBoxLayout();
    header->setSpacing(4);
    auto *title = makeLabel(QStringLiteral("SLIDES"), QStringLiteral("SectionTitle"));
    header->addWidget(title);
    m_count = makeLabel(QStringLiteral("0"), QStringLiteral("Faint"));
    header->addWidget(m_count);
    header->addStretch();

    auto *add = makeIconButton(Glyph::Plus, QStringLiteral("Add slide (Ctrl+Shift+N)"), this);
    auto *duplicate = makeIconButton(Glyph::Duplicate, QStringLiteral("Duplicate slide"), this);
    auto *left = makeIconButton(Glyph::ArrowLeft, QStringLiteral("Move slide left"), this);
    auto *right = makeIconButton(Glyph::ArrowRight, QStringLiteral("Move slide right"), this);
    auto *remove = makeIconButton(Glyph::Trash, QStringLiteral("Delete slide"), this);
    header->addWidget(add);
    header->addWidget(duplicate);
    header->addWidget(left);
    header->addWidget(right);
    header->addWidget(remove);
    layout->addLayout(header);

    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("SlideList"));
    m_list->setViewMode(QListView::IconMode);
    m_list->setFlow(QListView::LeftToRight);
    m_list->setWrapping(false);
    m_list->setResizeMode(QListView::Adjust);
    m_list->setMovement(QListView::Static);
    m_list->setUniformItemSizes(true);
    m_list->setGridSize(QSize(176, 136));
    m_list->setSpacing(2);
    m_list->setItemDelegate(new SlideDelegate(m_list));
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setMouseTracking(true);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    m_list->setFixedHeight(142);
    layout->addWidget(m_list);

    connect(add, &QToolButton::clicked, editor, &Editor::addSlide);
    connect(duplicate, &QToolButton::clicked, editor, &Editor::duplicateSlide);
    connect(remove, &QToolButton::clicked, editor, &Editor::deleteCurrentSlide);
    connect(left, &QToolButton::clicked, this, [this] {
        m_editor->moveSlide(m_editor->currentIndex(), m_editor->currentIndex() - 1);
    });
    connect(right, &QToolButton::clicked, this, [this] {
        m_editor->moveSlide(m_editor->currentIndex(), m_editor->currentIndex() + 1);
    });
    connect(m_list, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        if (!m_syncing)
            m_editor->setCurrentIndex(m_list->row(item));
    });
    connect(m_list, &QListWidget::customContextMenuRequested, this, &SlideStrip::showContextMenu);

    connect(editor, &Editor::presentationChanged, this, &SlideStrip::refresh);
    connect(editor, &Editor::presentationEdited, this, &SlideStrip::refresh);
    connect(editor, &Editor::slideIndexChanged, this, &SlideStrip::updateCurrent);
    connect(editor, &Editor::selectionChanged, this, &SlideStrip::updateCurrent);
    connect(editor, &Editor::modeChanged, this, &SlideStrip::refresh);

    refresh();
}

void SlideStrip::refresh()
{
    if (!m_editor->hasPresentation()) {
        m_list->clear();
        m_count->setText(QStringLiteral("0"));
        return;
    }

    const Presentation &deck = m_editor->presentation();
    bool sameSlides = m_list->count() == deck.slides.size();
    if (sameSlides) {
        for (int row = 0; row < m_list->count(); ++row) {
            if (m_list->item(row)->data(Qt::UserRole + 1).toString() != deck.slides.at(row).id) {
                sameSlides = false;
                break;
            }
        }
    }
    if (!sameSlides) {
        rebuild();
        return;
    }

    for (int row = 0; row < deck.slides.size(); ++row) {
        QListWidgetItem *item = m_list->item(row);
        const Slide &slide = deck.slides.at(row);
        item->setText(slide.name);
        item->setData(Qt::UserRole, thumbnailFor(slide));
        item->setData(Qt::UserRole + 2, m_editor->isPresenting() && row == m_editor->liveIndex());
        item->setToolTip(slide.name);
    }
    m_count->setText(QString::number(deck.slides.size()));
    updateCurrent();
    updateActions();
}

void SlideStrip::rebuild()
{
    const int scroll = m_list->horizontalScrollBar()->value();
    m_syncing = true;
    m_list->clear();
    const Presentation &deck = m_editor->presentation();
    for (int row = 0; row < deck.slides.size(); ++row) {
        const Slide &slide = deck.slides.at(row);
        auto *item = new QListWidgetItem(slide.name, m_list);
        item->setData(Qt::UserRole, thumbnailFor(slide));
        item->setData(Qt::UserRole + 1, slide.id);
        item->setData(Qt::UserRole + 2, m_editor->isPresenting() && row == m_editor->liveIndex());
        item->setToolTip(slide.name);
        item->setSizeHint(QSize(172, 132));
    }
    m_syncing = false;
    m_count->setText(QString::number(deck.slides.size()));
    updateCurrent();
    updateActions();
    m_list->horizontalScrollBar()->setValue(scroll);
}

void SlideStrip::updateCurrent()
{
    if (!m_editor->hasPresentation())
        return;
    m_syncing = true;
    const int index = qBound(0, m_editor->currentIndex(), m_list->count() - 1);
    if (m_list->count() > 0) {
        m_list->setCurrentRow(index);
        if (QListWidgetItem *item = m_list->item(index))
            m_list->scrollToItem(item, QAbstractItemView::EnsureVisible);
    }
    m_syncing = false;
    updateActions();
}

void SlideStrip::updateActions()
{
    const bool hasDeck = m_editor->hasPresentation();
    for (QToolButton *button : findChildren<QToolButton *>()) {
        if (!button->toolTip().contains(QStringLiteral("slide")))
            continue;
        if (button->toolTip().contains(QStringLiteral("Add")))
            button->setEnabled(hasDeck);
        else if (button->toolTip().contains(QStringLiteral("Duplicate")))
            button->setEnabled(hasDeck && m_editor->currentSlide() != nullptr);
        else if (button->toolTip().contains(QStringLiteral("Delete")))
            button->setEnabled(hasDeck);
        else if (button->toolTip().contains(QStringLiteral("left")))
            button->setEnabled(hasDeck && m_editor->currentIndex() > 0);
        else if (button->toolTip().contains(QStringLiteral("right")))
            button->setEnabled(hasDeck && m_editor->currentIndex() < m_list->count() - 1);
    }
}

void SlideStrip::showContextMenu(const QPoint &position)
{
    QListWidgetItem *item = m_list->itemAt(position);
    if (!item)
        return;
    m_list->setCurrentItem(item);
    const int index = m_list->row(item);
    QMenu menu(this);
    QAction *duplicate = menu.addAction(QStringLiteral("Duplicate slide"));
    QAction *moveLeft = menu.addAction(QStringLiteral("Move left"));
    QAction *moveRight = menu.addAction(QStringLiteral("Move right"));
    menu.addSeparator();
    QAction *remove = menu.addAction(QStringLiteral("Delete slide"));
    moveLeft->setEnabled(index > 0);
    moveRight->setEnabled(index < m_list->count() - 1);

    QAction *chosen = menu.exec(m_list->viewport()->mapToGlobal(position));
    if (chosen == duplicate)
        m_editor->duplicateSlide();
    else if (chosen == remove)
        m_editor->deleteCurrentSlide();
    else if (chosen == moveLeft)
        m_editor->moveSlide(index, index - 1);
    else if (chosen == moveRight)
        m_editor->moveSlide(index, index + 1);
}

QPixmap SlideStrip::thumbnailFor(const Slide &slide)
{
    const QByteArray slideJson = QJsonDocument(slide.toJson()).toJson(QJsonDocument::Compact);
    const QByteArray themeJson =
        QJsonDocument(m_editor->theme().toJson()).toJson(QJsonDocument::Compact);
    const QString key = QStringLiteral("%1:%2:%3")
                            .arg(slide.id)
                            .arg(QString::number(qHash(slideJson)),
                                 QString::number(qHash(themeJson)));
    const auto cached = m_thumbnails.constFind(key);
    if (cached != m_thumbnails.constEnd())
        return cached.value();

    RenderContext context;
    context.slideIndex = m_editor->presentation().indexOfSlide(slide.id);
    context.slideCount = m_editor->presentation().slides.size();
    context.presentationName = m_editor->presentation().name;
    context.timerSeconds = m_editor->timerRemainingSeconds();
    const QImage image = m_renderer->render(m_editor->presentation(), slide, m_editor->theme(),
                                            QSize(400, 225), context);
    QPixmap pixmap = QPixmap::fromImage(image);
    m_thumbnails.insert(key, pixmap);

    if (m_thumbnails.size() > 240) {
        m_thumbnails.clear();
        m_thumbnails.insert(key, pixmap);
    }
    return pixmap;
}

} // namespace mxu::ui
