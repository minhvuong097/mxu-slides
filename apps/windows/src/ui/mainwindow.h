#pragma once

#include <QList>
#include <QMainWindow>
#include <QSettings>

class QAction;
class QActionGroup;
class QCloseEvent;
class QEvent;
class QKeyEvent;
class QLabel;
class QMenu;
class QPushButton;
class QScreen;
class QSplitter;
class QStackedWidget;
class QTimer;
class QToolButton;
class QWidget;

namespace mxu {
class Editor;
class Library;
class SlideRenderer;
}

namespace mxu::ui {

class InspectorPanel;
class LibraryPanel;
class PresenterPage;
class ProgramWindow;
class SegmentedControl;
class SlideCanvas;
class SlideStrip;
class StageWindow;
class TimerChip;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    void openBundlePath(const QString &path);

protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void createActions();
    void buildMenus();
    QWidget *buildHeader();
    void buildWorkspace();
    QWidget *buildEditorPage();
    void buildStatusBar();
    void connectEditor();
    void restoreSession();

    void updateWindowTitle();
    void updateActions();
    void updateStatus();
    void updatePresentationUi();

    void newPresentation();
    void importPresentation();
    void exportPresentation();
    void savePresentation();
    void openSettings();
    bool applyLibraryRoot(const QString &root);

    void startPresenting();
    void stopPresenting();
    void togglePresenting();
    void ensureProgramWindow();
    void ensureStageWindow();
    void rebuildOutputMenu();
    void chooseOutputScreen(const QString &name);
    QScreen *resolveOutputScreen(const QString &name) const;
    QScreen *resolveStageScreen() const;
    void toggleStageDisplay(bool enabled);

    void editTextObject(const QString &objectId);
    void configureTimer();
    void goToSlideNumber(int number);
    void showShortcuts();
    void showAbout();
    void saveSession();
    void flushPendingWork();
    bool handlePresentationKey(QKeyEvent *event);

    QSettings m_settings;
    Editor *m_editor = nullptr;
    Library *m_library = nullptr;
    SlideRenderer *m_renderer = nullptr;

    LibraryPanel *m_libraryPanel = nullptr;
    SlideCanvas *m_canvas = nullptr;
    SlideStrip *m_strip = nullptr;
    InspectorPanel *m_inspector = nullptr;
    PresenterPage *m_presenter = nullptr;
    QSplitter *m_splitter = nullptr;
    QStackedWidget *m_stack = nullptr;
    QWidget *m_editorPage = nullptr;
    ProgramWindow *m_program = nullptr;
    StageWindow *m_stage = nullptr;

    SegmentedControl *m_modeSwitch = nullptr;
    QToolButton *m_undoButton = nullptr;
    QToolButton *m_redoButton = nullptr;
    QToolButton *m_stageButton = nullptr;
    QToolButton *m_outputButton = nullptr;
    QToolButton *m_settingsButton = nullptr;
    TimerChip *m_timerChip = nullptr;
    QPushButton *m_liveButton = nullptr;
    QLabel *m_saveState = nullptr;
    QLabel *m_zoomLabel = nullptr;
    QLabel *m_statusSlide = nullptr;
    QLabel *m_statusInfo = nullptr;

    QMenu *m_outputMenu = nullptr;
    QActionGroup *m_outputGroup = nullptr;

    QAction *m_newPresentationAction = nullptr;
    QAction *m_newSlideAction = nullptr;
    QAction *m_importAction = nullptr;
    QAction *m_exportAction = nullptr;
    QAction *m_saveAction = nullptr;
    QAction *m_settingsAction = nullptr;
    QAction *m_quitAction = nullptr;

    QAction *m_undoAction = nullptr;
    QAction *m_redoAction = nullptr;
    QAction *m_duplicateObjectAction = nullptr;
    QAction *m_deleteObjectAction = nullptr;
    QAction *m_editTextAction = nullptr;

    QAction *m_fitAction = nullptr;
    QAction *m_zoomInAction = nullptr;
    QAction *m_zoomOutAction = nullptr;

    QAction *m_presentAction = nullptr;
    QAction *m_nextAction = nullptr;
    QAction *m_previousAction = nullptr;
    QAction *m_blackoutAction = nullptr;
    QAction *m_logoAction = nullptr;
    QAction *m_stageAction = nullptr;
    QAction *m_timerToggleAction = nullptr;
    QAction *m_timerResetAction = nullptr;
    QAction *m_outputSettingsAction = nullptr;

    QAction *m_shortcutsAction = nullptr;
    QAction *m_aboutAction = nullptr;

    QString m_outputScreenName;
    QString m_stageScreenName;
    bool m_stageEnabled = false;
    QString m_lastPresentationId;
    QList<int> m_editSplitterSizes;

    QTimer *m_goToTimer = nullptr;
    QString m_goToBuffer;
    bool m_syncingMode = false;
};

} // namespace mxu::ui
