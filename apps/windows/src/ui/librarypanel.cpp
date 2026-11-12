#include "ui/librarypanel.h"

#include "core/editor.h"
#include "core/library.h"
#include "ui/icons.h"
#include "ui/theme.h"
#include "ui/widgets.h"

#include <QApplication>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QUrl>
#include <QVBoxLayout>

namespace mxu::ui {
namespace {

class LibraryDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override
    {
        return QSize(230, 55);
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        const Palette &p = mxu::ui::palette();
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        QRectF rect = option.rect.adjusted(4, 3, -4, -3);
        const bool selected = option.state & QStyle::State_Selected;
        const bool hovered = option.state & QStyle::State_MouseOver;
        if (selected || hovered) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(selected ? p.accentSoft : p.surface);
            painter->drawRoundedRect(rect, 8, 8);
        }
        if (selected) {
            painter->setPen(QPen(p.accent, 1));
            painter->setBrush(Qt::NoBrush);
            painter->drawRoundedRect(rect.adjusted(0.5, 0.5, -0.5, -0.5), 8, 8);
        }

        const QRectF markRect(rect.left() + 10, rect.top() + 13, 28, 28);
        paintAppMark(*painter, markRect);

        QFont nameFont = option.font;
        nameFont.setWeight(QFont::DemiBold);
        nameFont.setPointSizeF(10.2);
        painter->setFont(nameFont);
        painter->setPen(p.text);
        const QString name = index.data(Qt::DisplayRole).toString();
        const QRectF nameRect(rect.left() + 48, rect.top() + 9, rect.width() - 60, 19);
        painter->drawText(nameRect, Qt::AlignLeft | Qt::AlignVCenter,
                          painter->fontMetrics().elidedText(name, Qt::ElideRight,
                                                           int(nameRect.width())));

        QFont metaFont = option.font;
        metaFont.setPointSizeF(8.5);
        painter->setFont(metaFont);
        painter->setPen(selected ? p.textMuted : p.textFaint);
        const QString meta = index.data(Qt::UserRole + 1).toString();
        const QRectF metaRect(rect.left() + 48, rect.top() + 28, rect.width() - 58, 16);
        painter->drawText(metaRect, Qt::AlignLeft | Qt::AlignVCenter,
                          painter->fontMetrics().elidedText(meta, Qt::ElideRight,
                                                           int(metaRect.width())));

        if (!selected && index.data(Qt::UserRole + 2).toBool()) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(p.accent);
            painter->drawEllipse(QPointF(rect.right() - 12, rect.top() + 13), 3, 3);
        }
        painter->restore();
    }
};

} // namespace

LibraryPanel::LibraryPanel(Editor *editor, QWidget *parent)
    : QFrame(parent)
    , m_editor(editor)
{
    setObjectName(QStringLiteral("SidePanel"));
    setMinimumWidth(248);
    setMaximumWidth(360);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 14, 12, 12);
    layout->setSpacing(10);

    auto *header = new QHBoxLayout();
    header->setSpacing(4);
    auto *title = makeLabel(QStringLiteral("Library"), QStringLiteral("AppTitle"));
    header->addWidget(title);
    header->addStretch();
    auto *add = makeIconButton(Glyph::Plus, QStringLiteral("New presentation"), this);
    auto *import = makeIconButton(Glyph::Import, QStringLiteral("Import a .mxuslides file"), this);
    auto *folder = makeIconButton(Glyph::Folder, QStringLiteral("Open the library folder"), this);
    header->addWidget(add);
    header->addWidget(import);
    header->addWidget(folder);
    layout->addLayout(header);

    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(QStringLiteral("Search presentations"));
    m_search->setClearButtonEnabled(true);
    layout->addWidget(m_search);

    auto *metaRow = new QHBoxLayout();
    m_count = makeLabel(QStringLiteral("0 presentations"), QStringLiteral("Faint"));
    metaRow->addWidget(m_count);
    metaRow->addStretch();
    layout->addLayout(metaRow);

    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("LibraryList"));
    m_list->setItemDelegate(new LibraryDelegate(m_list));
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setUniformItemSizes(true);
    m_list->setMouseTracking(true);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    layout->addWidget(m_list, 1);

    m_empty = makeLabel(QStringLiteral("No presentations yet.\nCreate one or import a .mxuslides file."),
                        QStringLiteral("Faint"), this);
    m_empty->setAlignment(Qt::AlignCenter);
    m_empty->setWordWrap(true);
    m_empty->hide();
    layout->addWidget(m_empty);
    layout->addStretch();

    auto *hint = makeLabel(QStringLiteral("Presentations stay on this PC."), QStringLiteral("Faint"));
    hint->setAlignment(Qt::AlignCenter);
    layout->addWidget(hint);

    connect(add, &QToolButton::clicked, this, &LibraryPanel::newPresentation);
    connect(import, &QToolButton::clicked, this, &LibraryPanel::importBundle);
    connect(folder, &QToolButton::clicked, this, &LibraryPanel::revealLibrary);
    connect(m_search, &QLineEdit::textChanged, this, [this](const QString &text) {
        for (int row = 0; row < m_list->count(); ++row) {
            QListWidgetItem *item = m_list->item(row);
            const bool match = text.isEmpty()
                || item->data(Qt::DisplayRole).toString().contains(text, Qt::CaseInsensitive)
                || item->data(Qt::UserRole + 1).toString().contains(text, Qt::CaseInsensitive);
            item->setHidden(!match);
        }
    });
    connect(m_list, &QListWidget::itemActivated, this, [this](QListWidgetItem *) { openSelection(); });
    connect(m_list, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem *) { openSelection(); });
    connect(m_list, &QListWidget::customContextMenuRequested, this, &LibraryPanel::showContextMenu);
    connect(editor, &Editor::libraryChanged, this, &LibraryPanel::refresh);
    connect(editor, &Editor::presentationChanged, this, &LibraryPanel::refresh);
}

void LibraryPanel::refresh()
{
    const QString openId = m_editor->presentationId();
    QString selectedId;
    if (auto *current = m_list->currentItem())
        selectedId = current->data(Qt::UserRole).toString();
    if (selectedId.isEmpty())
        selectedId = openId;

    const QVector<LibraryEntry> entries = m_editor->library()->entries();
    m_list->clear();
    for (const LibraryEntry &entry : entries) {
        auto *item = new QListWidgetItem(entry.name, m_list);
        const QString folder = entry.folder.isEmpty() ? QStringLiteral("Unsorted") : entry.folder;
        const QString slides = entry.slideCount == 1 ? QStringLiteral("1 slide")
                                                     : QStringLiteral("%1 slides").arg(entry.slideCount);
        item->setData(Qt::UserRole, entry.id);
        item->setData(Qt::UserRole + 1, QStringLiteral("%1 · %2").arg(folder, slides));
        item->setData(Qt::UserRole + 2, entry.id == openId);
        item->setToolTip(QStringLiteral("%1\n%2\n%3")
                             .arg(entry.name, folder, slides));
        if (entry.id == selectedId)
            m_list->setCurrentItem(item);
    }

    m_count->setText(entries.size() == 1 ? QStringLiteral("1 presentation")
                                        : QStringLiteral("%1 presentations").arg(entries.size()));
    const bool empty = entries.isEmpty();
    m_list->setVisible(!empty);
    m_empty->setVisible(empty);
    if (entries.isEmpty())
        m_empty->show();

    const QString query = m_search->text();
    if (!query.isEmpty()) {
        for (int row = 0; row < m_list->count(); ++row) {
            QListWidgetItem *item = m_list->item(row);
            const bool match = item->data(Qt::DisplayRole).toString().contains(query, Qt::CaseInsensitive)
                || item->data(Qt::UserRole + 1).toString().contains(query, Qt::CaseInsensitive);
            item->setHidden(!match);
        }
    }
}

void LibraryPanel::openSelection()
{
    auto *item = m_list->currentItem();
    if (!item)
        return;
    const QString id = item->data(Qt::UserRole).toString();
    QString error;
    if (!m_editor->open(id, &error)) {
        QMessageBox::warning(this, QStringLiteral("Open presentation"), error);
        refresh();
    }
}

void LibraryPanel::newPresentation()
{
    bool ok = false;
    const QString name = QInputDialog::getText(this, QStringLiteral("New presentation"),
                                               QStringLiteral("Presentation name:"),
                                               QLineEdit::Normal,
                                               QStringLiteral("Untitled"), &ok);
    if (!ok)
        return;
    m_editor->createPresentation(name);
    refresh();
}

void LibraryPanel::importBundle()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Import presentation"), QString(),
        QStringLiteral("MxU Slides presentation (*.mxuslides);;All files (*)"));
    if (path.isEmpty())
        return;
    QString error;
    if (!m_editor->importBundle(path, &error))
        QMessageBox::warning(this, QStringLiteral("Import presentation"), error);
    refresh();
}

void LibraryPanel::showContextMenu(const QPoint &position)
{
    auto *item = m_list->itemAt(position);
    if (!item)
        return;
    m_list->setCurrentItem(item);
    const QString id = item->data(Qt::UserRole).toString();
    const QString name = item->data(Qt::DisplayRole).toString();

    QMenu menu(this);
    QAction *open = menu.addAction(QStringLiteral("Open"));
    QAction *duplicate = menu.addAction(QStringLiteral("Duplicate"));
    menu.addSeparator();
    QAction *reveal = menu.addAction(QStringLiteral("Show in Explorer"));
    QAction *remove = menu.addAction(QStringLiteral("Delete…"));
    QAction *chosen = menu.exec(m_list->viewport()->mapToGlobal(position));
    if (chosen == open)
        openSelection();
    else if (chosen == duplicate)
        duplicateEntry(id);
    else if (chosen == reveal)
        revealLibrary();
    else if (chosen == remove)
        deleteEntry(id, name);
}

void LibraryPanel::deleteEntry(const QString &id, const QString &name)
{
    const auto answer = QMessageBox::question(
        this, QStringLiteral("Delete presentation"),
        QStringLiteral("Delete “%1” from the library?\n\nThis cannot be undone.").arg(name),
        QMessageBox::Cancel | QMessageBox::Yes, QMessageBox::Cancel);
    if (answer != QMessageBox::Yes)
        return;

    QString error;
    if (!m_editor->library()->deletePresentation(id, &error)) {
        QMessageBox::warning(this, QStringLiteral("Delete presentation"), error);
        return;
    }
    if (m_editor->presentationId() == id) {
        m_editor->close();
        const QVector<LibraryEntry> entries = m_editor->library()->entries();
        if (!entries.isEmpty())
            m_editor->open(entries.first().id);
    }
    refresh();
}

void LibraryPanel::duplicateEntry(const QString &id)
{
    QString error;
    const QString copyId = m_editor->library()->duplicatePresentation(id, &error);
    if (copyId.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Duplicate presentation"), error);
        return;
    }
    refresh();
    m_editor->open(copyId, &error);
}

void LibraryPanel::revealLibrary()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_editor->library()->rootPath()));
}

} // namespace mxu::ui
