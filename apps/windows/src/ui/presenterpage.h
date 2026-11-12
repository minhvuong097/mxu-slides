#pragma once

#include <QWidget>

class QLabel;
class QPlainTextEdit;
class QPushButton;
class QTimer;
class QToolButton;

namespace mxu {
class Editor;
class SlideRenderer;
}

namespace mxu::ui {

class PreviewWidget;

class PresenterPage : public QWidget {
    Q_OBJECT
public:
    explicit PresenterPage(Editor *editor, SlideRenderer *renderer, QWidget *parent = nullptr);

    void refresh();

signals:
    void outputSettingsRequested();

private:
    void createUi();
    void updateClockAndTimer();

    Editor *m_editor = nullptr;
    SlideRenderer *m_renderer = nullptr;
    PreviewWidget *m_program = nullptr;
    PreviewWidget *m_next = nullptr;
    QLabel *m_title = nullptr;
    QLabel *m_position = nullptr;
    QLabel *m_nextLabel = nullptr;
    QLabel *m_clock = nullptr;
    QLabel *m_timerValue = nullptr;
    QPlainTextEdit *m_notes = nullptr;
    QPushButton *m_previous = nullptr;
    QPushButton *m_nextButton = nullptr;
    QPushButton *m_blackout = nullptr;
    QPushButton *m_logo = nullptr;
    QPushButton *m_stop = nullptr;
    QPushButton *m_outputSettings = nullptr;
    QToolButton *m_timerToggle = nullptr;
    QToolButton *m_timerReset = nullptr;
    QTimer *m_tick = nullptr;
};

} // namespace mxu::ui
