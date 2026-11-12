#pragma once

#include <QFrame>
#include <QHash>
#include <QPixmap>

class QLabel;
class QListWidget;

namespace mxu {
class Editor;
class Slide;
class SlideRenderer;
}

namespace mxu::ui {

class SlideStrip : public QFrame {
    Q_OBJECT
public:
    explicit SlideStrip(Editor *editor, SlideRenderer *renderer, QWidget *parent = nullptr);

    void refresh();

private:
    void rebuild();
    void updateCurrent();
    void updateActions();
    void showContextMenu(const QPoint &position);
    QPixmap thumbnailFor(const Slide &slide);

    Editor *m_editor = nullptr;
    SlideRenderer *m_renderer = nullptr;
    QListWidget *m_list = nullptr;
    QLabel *m_count = nullptr;
    QHash<QString, QPixmap> m_thumbnails;
    bool m_syncing = false;
};

} // namespace mxu::ui
