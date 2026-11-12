#pragma once

#include <QImage>
#include <QWidget>

class QPainter;
class QScreen;
class QTimer;
class QVariantAnimation;

namespace mxu {
class Editor;
class SlideRenderer;
}

namespace mxu::ui {

class ProgramWindow : public QWidget {
    Q_OBJECT
public:
    explicit ProgramWindow(Editor *editor, SlideRenderer *renderer, QWidget *parent = nullptr);
    ~ProgramWindow() override;

    void setTargetScreen(QScreen *screen);
    QScreen *targetScreen() const { return m_screen; }
    void setPresenting(bool presenting);
    void refresh();

signals:
    void stopRequested();

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    void configureWindow();
    QImage renderSlideImage(int index) const;
    void paintFrame(QPainter &painter, const QImage &image, qreal opacity) const;

    Editor *m_editor = nullptr;
    SlideRenderer *m_renderer = nullptr;
    QScreen *m_screen = nullptr;
    bool m_presenting = false;

    QImage m_current;
    QImage m_previous;
    qreal m_fade = 1.0;
    QVariantAnimation *m_fadeAnimation = nullptr;
    QTimer *m_clock = nullptr;
    int m_lastIndex = -1;
};

} // namespace mxu::ui
