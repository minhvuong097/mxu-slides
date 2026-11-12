#pragma once

#include <QFrame>

class QLabel;
class QLineEdit;
class QListWidget;

namespace mxu {
class Editor;
}

namespace mxu::ui {

class LibraryPanel : public QFrame {
    Q_OBJECT
public:
    explicit LibraryPanel(Editor *editor, QWidget *parent = nullptr);
    void refresh();

private:
    void openSelection();
    void newPresentation();
    void importBundle();
    void showContextMenu(const QPoint &position);
    void deleteEntry(const QString &id, const QString &name);
    void duplicateEntry(const QString &id);
    void revealLibrary();

    Editor *m_editor = nullptr;
    QLineEdit *m_search = nullptr;
    QListWidget *m_list = nullptr;
    QLabel *m_count = nullptr;
    QLabel *m_empty = nullptr;
};

} // namespace mxu::ui
