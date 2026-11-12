#pragma once

#include <QWidget>

class QScreen;
class QTimer;

namespace mxu {
class Editor;
class SlideRenderer;
}

namespace mxu::ui {

class StageWindow : public QWidget {
    Q_OBJECT
public:
    explicit StageWindow(Editor *editor, SlideRenderer *renderer, QWidget *parent = nullptr);

    void setTargetScreen(QScreen *screen);
    QScreen *targetScreen() const { return m_screen; }
    void setActive(bool active);
    void refresh();

protected:
    void paintEvent(QPaintEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    void configureWindow();

    Editor *m_editor = nullptr;
    SlideRenderer *m_renderer = nullptr;
    QScreen *m_screen = nullptr;
    bool m_active = false;
    QTimer *m_clock = nullptr;
};

} // namespace mxu::ui
