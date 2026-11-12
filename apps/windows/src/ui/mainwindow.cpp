#include "ui/mainwindow.h"

#include "core/editor.h"
#include "core/library.h"
#include "render/sliderenderer.h"
#include "ui/icons.h"
#include "ui/inspectorpanel.h"
#include "ui/librarypanel.h"
#include "ui/presenterpage.h"
#include "ui/programwindow.h"
#include "ui/settingsdialog.h"
#include "ui/slidecanvas.h"
#include "ui/slidestrip.h"
#include "ui/stagewindow.h"
#include "ui/theme.h"
#include "ui/widgets.h"

#include <QAbstractSpinBox>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScreen>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

namespace mxu::ui {
namespace {

QString safeFileName(const QString &name)
{
    QString safe = name.trimmed();
    if (safe.isEmpty())
        safe = QStringLiteral("Presentation");
    safe.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")), QStringLiteral("-"));
    return safe;
}

QKeySequence key(const QString &text)
{
    return QKeySequence::fromString(text, QKeySequence::PortableText);
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_settings(QStringLiteral("MxU"), QStringLiteral("MxU Slides"))
{
    setWindowIcon(appIcon());
    setWindowTitle(QStringLiteral("MxU Slides"));
    resize(1480, 920);
    setMinimumSize(1120, 720);

    m_outputScreenName = m_settings.value(QStringLiteral("output/screen")).toString();
    m_stageScreenName = m_settings.value(QStringLiteral("stage/screen")).toString();
    m_stageEnabled = m_settings.value(QStringLiteral("stage/enabled"), false).toBool();

    const QString libraryRoot =
        m_settings.value(QStringLiteral("library/root"), Library::defaultRootPath()).toString();
    m_library = new Library(libraryRoot.isEmpty() ? Library::defaultRootPath() : libraryRoot);
    QString error;
    if (!m_library->ensureLayout(&error)) {
        QMessageBox::critical(this, QStringLiteral("MxU Slides library"), error);
    }
    m_library->seedStarterContent();

    m_editor = new Editor(m_library, this);
    m_renderer = new SlideRenderer;
    m_renderer->setMediaResolver([this](const QString &mediaId) {
        return m_editor->mediaPath(mediaId);
    });
    m_editor->setTimerTotalSeconds(
        m_settings.value(QStringLiteral("timer/seconds"), 300).toInt());

    createActions();
    buildMenus();
    buildWorkspace();
    buildStatusBar();
    connectEditor();

    m_goToTimer = new QTimer(this);
    m_goToTimer->setSingleShot(true);
    m_goToTimer->setInterval(1600);
    connect(m_goToTimer, &QTimer::timeout, this, [this] {
        m_goToBuffer.clear();
        statusBar()->clearMessage();
    });

    connect(qApp, &QGuiApplication::screenAdded, this,
            [this](QScreen *) { rebuildOutputMenu(); });
    connect(qApp, &QGuiApplication::screenRemoved, this, [this](QScreen *screen) {
        const bool outputRemoved = screen == resolveOutputScreen(m_outputScreenName);
        if (outputRemoved && m_program) {
            m_program->setTargetScreen(nullptr);
            m_program->setPresenting(m_editor->isPresenting());
        }
        if (m_stage && m_stage->targetScreen() == screen)
            m_stage->setTargetScreen(nullptr);
        QTimer::singleShot(0, this, &MainWindow::rebuildOutputMenu);
    });

    restoreSession();
    restoreGeometry(m_settings.value(QStringLiteral("window/geometry")).toByteArray());
    if (m_settings.contains(QStringLiteral("window/splitter")))
        m_splitter->restoreState(m_settings.value(QStringLiteral("window/splitter")).toByteArray());

    qApp->installEventFilter(this);
}

MainWindow::~MainWindow()
{
    qApp->removeEventFilter(this);
    delete m_program;
    m_program = nullptr;
    delete m_stage;
    m_stage = nullptr;
}

void MainWindow::createActions()
{
    const Palette &p = mxu::ui::palette();

    m_newPresentationAction = new QAction(ui::icon(Glyph::Plus, p.textMuted, 16),
                                          QStringLiteral("New presentation…"), this);
    m_newPresentationAction->setShortcut(QKeySequence::New);
    connect(m_newPresentationAction, &QAction::triggered, this, &MainWindow::newPresentation);

    m_newSlideAction = new QAction(ui::icon(Glyph::Grid, p.textMuted, 16),
                                   QStringLiteral("New slide"), this);
    m_newSlideAction->setShortcut(key(QStringLiteral("Ctrl+Shift+N")));
    connect(m_newSlideAction, &QAction::triggered, m_editor, &Editor::addSlide);

    m_importAction = new QAction(ui::icon(Glyph::Import, p.textMuted, 16),
                                 QStringLiteral("Import .mxuslides…"), this);
    m_importAction->setShortcut(QKeySequence::Open);
    connect(m_importAction, &QAction::triggered, this, &MainWindow::importPresentation);

    m_exportAction = new QAction(ui::icon(Glyph::Export, p.textMuted, 16),
                                 QStringLiteral("Export .mxuslides…"), this);
    m_exportAction->setShortcut(key(QStringLiteral("Ctrl+Shift+E")));
    connect(m_exportAction, &QAction::triggered, this, &MainWindow::exportPresentation);

    m_saveAction = new QAction(ui::icon(Glyph::Save, p.textMuted, 16),
                               QStringLiteral("Save"), this);
    m_saveAction->setShortcut(QKeySequence::Save);
    connect(m_saveAction, &QAction::triggered, this, &MainWindow::savePresentation);

    m_settingsAction = new QAction(ui::icon(Glyph::Settings, p.textMuted, 16),
                                   QStringLiteral("Settings…"), this);
    m_settingsAction->setShortcut(key(QStringLiteral("Ctrl+,")));
    connect(m_settingsAction, &QAction::triggered, this, &MainWindow::openSettings);

    m_quitAction = new QAction(QStringLiteral("Exit"), this);
    m_quitAction->setShortcut(QKeySequence::Quit);
    connect(m_quitAction, &QAction::triggered, this, &QWidget::close);

    m_undoAction = new QAction(ui::icon(Glyph::Undo, p.textMuted, 16),
                               QStringLiteral("Undo"), this);
    m_undoAction->setShortcut(QKeySequence::Undo);
    connect(m_undoAction, &QAction::triggered, m_editor, &Editor::undo);

    m_redoAction = new QAction(ui::icon(Glyph::Redo, p.textMuted, 16),
                               QStringLiteral("Redo"), this);
    m_redoAction->setShortcut(QKeySequence::Redo);
    connect(m_redoAction, &QAction::triggered, m_editor, &Editor::redo);

    m_duplicateObjectAction = new QAction(ui::icon(Glyph::Duplicate, p.textMuted, 16),
                                          QStringLiteral("Duplicate object"), this);
    m_duplicateObjectAction->setShortcut(key(QStringLiteral("Ctrl+D")));
    connect(m_duplicateObjectAction, &QAction::triggered, m_editor,
            &Editor::duplicateSelectedObject);

    m_deleteObjectAction = new QAction(ui::icon(Glyph::Trash, p.live, 16),
                                       QStringLiteral("Delete object"), this);
    connect(m_deleteObjectAction, &QAction::triggered, m_editor, &Editor::deleteSelectedObject);

    m_editTextAction = new QAction(QStringLiteral("Edit text…"), this);
    m_editTextAction->setShortcut(key(QStringLiteral("F2")));
    connect(m_editTextAction, &QAction::triggered, this, [this] {
        if (const SlideObject *object = m_editor->selectedObject()) {
            if (object->isText())
                editTextObject(object->id);
        }
    });

    m_fitAction = new QAction(ui::icon(Glyph::Fit, p.textMuted, 16),
                              QStringLiteral("Fit slide"), this);
    m_fitAction->setShortcut(key(QStringLiteral("Ctrl+0")));
    connect(m_fitAction, &QAction::triggered, this, [this] { m_canvas->fitToWindow(); });

    m_zoomInAction = new QAction(ui::icon(Glyph::ZoomIn, p.textMuted, 16),
                                 QStringLiteral("Zoom in"), this);
    m_zoomInAction->setShortcut(QKeySequence::ZoomIn);
    connect(m_zoomInAction, &QAction::triggered, this, [this] { m_canvas->zoomIn(); });

    m_zoomOutAction = new QAction(ui::icon(Glyph::ZoomOut, p.textMuted, 16),
                                  QStringLiteral("Zoom out"), this);
    m_zoomOutAction->setShortcut(QKeySequence::ZoomOut);
    connect(m_zoomOutAction, &QAction::triggered, this, [this] { m_canvas->zoomOut(); });

    m_presentAction = new QAction(ui::icon(Glyph::Play, p.textMuted, 16),
                                  QStringLiteral("Start presenting"), this);
    m_presentAction->setShortcut(key(QStringLiteral("F5")));
    connect(m_presentAction, &QAction::triggered, this, &MainWindow::togglePresenting);

    m_nextAction = new QAction(ui::icon(Glyph::ArrowRight, p.textMuted, 16),
                               QStringLiteral("Next slide"), this);
    connect(m_nextAction, &QAction::triggered, m_editor, &Editor::next);
    m_previousAction = new QAction(ui::icon(Glyph::ArrowLeft, p.textMuted, 16),
                                   QStringLiteral("Previous slide"), this);
    connect(m_previousAction, &QAction::triggered, m_editor, &Editor::previous);

    m_blackoutAction = new QAction(ui::icon(Glyph::Blackout, p.textMuted, 16),
                                   QStringLiteral("Blackout"), this);
    m_blackoutAction->setCheckable(true);
    m_blackoutAction->setShortcut(key(QStringLiteral("B")));
    connect(m_blackoutAction, &QAction::triggered, this,
            [this](bool checked) { m_editor->setBlackout(checked); });

    m_logoAction = new QAction(ui::icon(Glyph::Logo, p.textMuted, 16),
                               QStringLiteral("Logo screen"), this);
    m_logoAction->setCheckable(true);
    m_logoAction->setShortcut(key(QStringLiteral("L")));
    connect(m_logoAction, &QAction::triggered, this,
            [this](bool checked) { m_editor->setLogoVisible(checked); });

    m_stageAction = new QAction(ui::icon(Glyph::Stage, p.textMuted, 16),
                                QStringLiteral("Stage display"), this);
    m_stageAction->setCheckable(true);
    connect(m_stageAction, &QAction::toggled, this, &MainWindow::toggleStageDisplay);

    m_timerToggleAction = new QAction(ui::icon(Glyph::Play, p.textMuted, 16),
                                      QStringLiteral("Start timer"), this);
    connect(m_timerToggleAction, &QAction::triggered, this, [this] {
        if (m_editor->timerRunning())
            m_editor->pauseTimer();
        else
            m_editor->startTimer();
    });
    m_timerResetAction = new QAction(ui::icon(Glyph::Reset, p.textMuted, 16),
                                     QStringLiteral("Reset timer"), this);
    connect(m_timerResetAction, &QAction::triggered, m_editor, &Editor::resetTimer);

    m_outputSettingsAction = new QAction(ui::icon(Glyph::Screen, p.textMuted, 16),
                                         QStringLiteral("Output settings…"), this);
    connect(m_outputSettingsAction, &QAction::triggered, this, &MainWindow::openSettings);

    m_shortcutsAction = new QAction(QStringLiteral("Keyboard shortcuts"), this);
    m_shortcutsAction->setShortcut(QKeySequence::HelpContents);
    connect(m_shortcutsAction, &QAction::triggered, this, &MainWindow::showShortcuts);

    m_aboutAction = new QAction(QStringLiteral("About MxU Slides"), this);
    connect(m_aboutAction, &QAction::triggered, this, &MainWindow::showAbout);
}

void MainWindow::buildMenus()
{
    menuBar()->setNativeMenuBar(false);

    QMenu *file = menuBar()->addMenu(QStringLiteral("&File"));
    file->addAction(m_newPresentationAction);
    file->addAction(m_newSlideAction);
    file->addSeparator();
    file->addAction(m_importAction);
    file->addAction(m_exportAction);
    file->addAction(m_saveAction);
    file->addSeparator();
    file->addAction(m_settingsAction);
    file->addAction(m_quitAction);

    QMenu *edit = menuBar()->addMenu(QStringLiteral("&Edit"));
    edit->addAction(m_undoAction);
    edit->addAction(m_redoAction);
    edit->addSeparator();
    edit->addAction(m_editTextAction);
    edit->addAction(m_duplicateObjectAction);
    edit->addAction(m_deleteObjectAction);

    QMenu *view = menuBar()->addMenu(QStringLiteral("&View"));
    view->addAction(m_fitAction);
    view->addAction(m_zoomInAction);
    view->addAction(m_zoomOutAction);

    QMenu *present = menuBar()->addMenu(QStringLiteral("&Present"));
    present->addAction(m_presentAction);
    present->addAction(m_previousAction);
    present->addAction(m_nextAction);
    present->addSeparator();
    present->addAction(m_blackoutAction);
    present->addAction(m_logoAction);
    present->addAction(m_stageAction);
    present->addSeparator();
    present->addAction(m_timerToggleAction);
    present->addAction(m_timerResetAction);
    present->addAction(m_outputSettingsAction);

    QMenu *help = menuBar()->addMenu(QStringLiteral("&Help"));
    help->addAction(m_shortcutsAction);
    help->addAction(m_aboutAction);

    m_outputMenu = new QMenu(QStringLiteral("Program output"), this);
    rebuildOutputMenu();
}

QWidget *MainWindow::buildHeader()
{
    const Palette &p = mxu::ui::palette();
    auto *header = new QFrame(this);
    header->setObjectName(QStringLiteral("Header"));
    header->setFixedHeight(58);
    auto *layout = new QHBoxLayout(header);
    layout->setContentsMargins(14, 7, 12, 7);
    layout->setSpacing(7);

    auto *mark = new QLabel(header);
    mark->setPixmap(appMarkPixmap(30));
    layout->addWidget(mark);

    auto *brand = new QVBoxLayout;
    brand->setSpacing(0);
    brand->addWidget(makeLabel(QStringLiteral("MxU Slides"), QStringLiteral("AppTitle")));
    brand->addWidget(makeLabel(QStringLiteral("WINDOWS · QT 6.12"),
                               QStringLiteral("SectionTitle")));
    layout->addLayout(brand);
    layout->addSpacing(9);

    m_modeSwitch = new SegmentedControl(header);
    m_modeSwitch->addItem(QStringLiteral("Edit"), QStringLiteral("edit"));
    m_modeSwitch->addItem(QStringLiteral("Present"), QStringLiteral("present"));
    m_modeSwitch->setCurrentValue(QStringLiteral("edit"));
    layout->addWidget(m_modeSwitch);

    m_saveState = makeLabel(QStringLiteral("Saved"), QStringLiteral("Faint"));
    layout->addWidget(m_saveState);
    layout->addStretch();

    m_undoButton = makeIconButton(Glyph::Undo, QStringLiteral("Undo"), header);
    m_undoButton->setDefaultAction(m_undoAction);
    layout->addWidget(m_undoButton);
    m_redoButton = makeIconButton(Glyph::Redo, QStringLiteral("Redo"), header);
    m_redoButton->setDefaultAction(m_redoAction);
    layout->addWidget(m_redoButton);

    m_timerChip = new TimerChip(header);
    layout->addWidget(m_timerChip);
    layout->addSpacing(3);

    m_stageButton = makeIconButton(Glyph::Stage, QStringLiteral("Stage display"), header);
    m_stageButton->setDefaultAction(m_stageAction);
    layout->addWidget(m_stageButton);

    m_outputButton = makeIconButton(Glyph::Screen, QStringLiteral("Program output display"),
                                    header);
    m_outputButton->setMenu(m_outputMenu);
    m_outputButton->setPopupMode(QToolButton::InstantPopup);
    layout->addWidget(m_outputButton);

    m_liveButton = new QPushButton(QStringLiteral("Go live"), header);
    m_liveButton->setObjectName(QStringLiteral("Live"));
    m_liveButton->setIcon(ui::icon(Glyph::Play, Qt::white, 15));
    m_liveButton->setMinimumWidth(92);
    m_liveButton->setCursor(Qt::PointingHandCursor);
    m_liveButton->setToolTip(QStringLiteral("Start presenting (F5)"));
    layout->addWidget(m_liveButton);

    m_settingsButton = makeIconButton(Glyph::Settings, QStringLiteral("Settings"), header);
    m_settingsButton->setDefaultAction(m_settingsAction);
    layout->addWidget(m_settingsButton);

    connect(m_modeSwitch, &SegmentedControl::changed, this, [this](const QString &value) {
        if (m_syncingMode)
            return;
        if (value == QLatin1String("present"))
            startPresenting();
        else
            stopPresenting();
    });
    connect(m_liveButton, &QPushButton::clicked, this, &MainWindow::togglePresenting);
    connect(m_timerChip, &TimerChip::toggleRequested, this, [this] {
        if (m_editor->timerRunning())
            m_editor->pauseTimer();
        else
            m_editor->startTimer();
    });
    connect(m_timerChip, &TimerChip::resetRequested, m_editor, &Editor::resetTimer);
    connect(m_timerChip, &TimerChip::configureRequested, this, &MainWindow::configureTimer);

    return header;
}

QWidget *MainWindow::buildEditorPage()
{
    const Palette &p = mxu::ui::palette();
    auto *page = new QWidget;
    page->setObjectName(QStringLiteral("EditorPage"));
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *toolbar = new QFrame(page);
    toolbar->setObjectName(QStringLiteral("EditorToolbar"));
    toolbar->setFixedHeight(48);
    toolbar->setStyleSheet(
        QStringLiteral("QFrame#EditorToolbar { background: %1; border-bottom: 1px solid %2; }")
            .arg(p.panel.name(), p.border.name()));
    auto *bar = new QHBoxLayout(toolbar);
    bar->setContentsMargins(12, 7, 12, 7);
    bar->setSpacing(4);

    m_canvas = new SlideCanvas(m_editor, m_renderer, page);
    m_strip = new SlideStrip(m_editor, m_renderer, page);

    auto makeTool = [this, toolbar](Glyph glyph, const QString &text, const QString &tip) {
        auto *button = new QToolButton(toolbar);
        button->setText(text);
        button->setIcon(ui::icon(glyph, mxu::ui::palette().textMuted, 16));
        button->setIconSize(QSize(16, 16));
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        button->setToolTip(tip);
        button->setCursor(Qt::PointingHandCursor);
        return button;
    };

    auto *addText = makeTool(Glyph::Text, QStringLiteral("Text"), QStringLiteral("Add a text box"));
    auto *addShape = makeTool(Glyph::Shape, QStringLiteral("Shape"),
                              QStringLiteral("Add a shape"));
    auto *addImage = makeTool(Glyph::Image, QStringLiteral("Image"),
                              QStringLiteral("Add an image"));
    auto *addSlide = makeTool(Glyph::Plus, QStringLiteral("Slide"),
                              QStringLiteral("Add a slide (Ctrl+Shift+N)"));
    bar->addWidget(addText);
    bar->addWidget(addShape);
    bar->addWidget(addImage);
    bar->addSpacing(6);
    bar->addWidget(addSlide);
    bar->addStretch();

    m_zoomLabel = makeLabel(QStringLiteral("100%"), QStringLiteral("Faint"));
    m_zoomLabel->setMinimumWidth(40);
    m_zoomLabel->setAlignment(Qt::AlignCenter);
    bar->addWidget(m_zoomLabel);
    auto *fit = makeIconButton(Glyph::Fit, QStringLiteral("Fit slide (Ctrl+0)"), toolbar);
    fit->setDefaultAction(m_fitAction);
    auto *zoomOut = makeIconButton(Glyph::ZoomOut, QStringLiteral("Zoom out"), toolbar);
    zoomOut->setDefaultAction(m_zoomOutAction);
    auto *zoomIn = makeIconButton(Glyph::ZoomIn, QStringLiteral("Zoom in"), toolbar);
    zoomIn->setDefaultAction(m_zoomInAction);
    bar->addWidget(fit);
    bar->addWidget(zoomOut);
    bar->addWidget(zoomIn);

    connect(addText, &QToolButton::clicked, m_canvas, &SlideCanvas::addTextObject);
    connect(addShape, &QToolButton::clicked, m_canvas, &SlideCanvas::addShapeObject);
    connect(addImage, &QToolButton::clicked, m_canvas, &SlideCanvas::addImageObject);
    connect(addSlide, &QToolButton::clicked, m_editor, &Editor::addSlide);
    connect(m_canvas, &SlideCanvas::zoomChanged, this, [this](int percent) {
        m_zoomLabel->setText(QStringLiteral("%1%").arg(percent));
    });
    connect(m_canvas, &SlideCanvas::textEditingRequested, this, &MainWindow::editTextObject);

    layout->addWidget(toolbar);
    layout->addWidget(m_canvas, 1);
    layout->addWidget(m_strip);
    return page;
}

void MainWindow::buildWorkspace()
{
    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(buildHeader());

    m_splitter = new QSplitter(Qt::Horizontal);
    m_libraryPanel = new LibraryPanel(m_editor);
    m_stack = new QStackedWidget;
    m_editorPage = buildEditorPage();
    m_stack->addWidget(m_editorPage);
    m_presenter = new PresenterPage(m_editor, m_renderer);
    m_stack->addWidget(m_presenter);
    m_inspector = new InspectorPanel(m_editor, m_renderer);

    m_splitter->addWidget(m_libraryPanel);
    m_splitter->addWidget(m_stack);
    m_splitter->addWidget(m_inspector);
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setStretchFactor(2, 0);
    m_splitter->setChildrenCollapsible(false);
    m_splitter->setSizes({270, 900, 330});
    layout->addWidget(m_splitter, 1);

    connect(m_presenter, &PresenterPage::outputSettingsRequested, this,
            &MainWindow::openSettings);

    setCentralWidget(central);
}

void MainWindow::buildStatusBar()
{
    statusBar()->setSizeGripEnabled(false);
    m_statusSlide = makeLabel(QStringLiteral("No presentation"), QStringLiteral("Faint"));
    m_statusInfo = makeLabel(QStringLiteral("Qt %1 · Windows").arg(qVersion()),
                             QStringLiteral("Faint"));
    statusBar()->addPermanentWidget(m_statusSlide);
    statusBar()->addPermanentWidget(makeLabel(QStringLiteral("  ·  "), QStringLiteral("Faint")));
    statusBar()->addPermanentWidget(m_statusInfo);
    statusBar()->showMessage(QStringLiteral("Ready"));
}

void MainWindow::connectEditor()
{
    connect(m_editor, &Editor::presentationChanged, this, [this] {
        updateWindowTitle();
        updateActions();
        updateStatus();
    });
    connect(m_editor, &Editor::presentationEdited, this, [this] {
        updateWindowTitle();
        updateActions();
        updateStatus();
    });
    connect(m_editor, &Editor::slideIndexChanged, this, [this] {
        updateActions();
        updateStatus();
    });
    connect(m_editor, &Editor::selectionChanged, this, [this] { updateActions(); });
    connect(m_editor, &Editor::liveChanged, this, [this] {
        updateActions();
        updateStatus();
    });
    connect(m_editor, &Editor::modeChanged, this, [this] { updatePresentationUi(); });
    connect(m_editor, &Editor::dirtyChanged, this, [this] {
        updateWindowTitle();
        updateActions();
    });
    connect(m_editor, &Editor::timerChanged, this, &MainWindow::updateActions);
    connect(m_editor, &Editor::undoStateChanged, this, &MainWindow::updateActions);
    connect(m_editor, &Editor::statusMessage, this, [this](const QString &message, int timeout) {
        statusBar()->showMessage(message, timeout);
    });
    connect(m_editor, &Editor::libraryChanged, this, [this] { updateWindowTitle(); });
}

void MainWindow::restoreSession()
{
    const QString last = m_settings.value(QStringLiteral("session/lastPresentation")).toString();
    const QVector<LibraryEntry> entries = m_library->entries();
    QString openId;
    if (!last.isEmpty()) {
        const auto match = std::find_if(entries.begin(), entries.end(),
                                        [&last](const LibraryEntry &entry) {
                                            return entry.id == last;
                                        });
        if (match != entries.end())
            openId = match->id;
    }
    if (openId.isEmpty() && !entries.isEmpty())
        openId = entries.first().id;

    QString error;
    if (!openId.isEmpty() && !m_editor->open(openId, &error))
        statusBar()->showMessage(error, 5000);
    if (!m_editor->hasPresentation()) {
        m_editor->createPresentation(QStringLiteral("Welcome"));
        if (!m_editor->hasPresentation())
            statusBar()->showMessage(QStringLiteral("Could not create a presentation"), 5000);
    }

    updateWindowTitle();
    updateActions();
    updateStatus();
    updatePresentationUi();
}

void MainWindow::updateWindowTitle()
{
    const bool hasDeck = m_editor->hasPresentation();
    if (!hasDeck) {
        setWindowTitle(QStringLiteral("MxU Slides"));
        m_lastPresentationId.clear();
        return;
    }

    const QString id = m_editor->presentationId();
    if (id != m_lastPresentationId) {
        m_lastPresentationId = id;
        m_settings.setValue(QStringLiteral("session/lastPresentation"), id);
    }
    setWindowTitle(QStringLiteral("%1%2 — MxU Slides")
                       .arg(m_editor->presentation().name,
                            m_editor->dirty() ? QStringLiteral(" *") : QString()));
}

void MainWindow::updateActions()
{
    if (!m_editor)
        return;
    const bool hasDeck = m_editor->hasPresentation();
    const bool presenting = m_editor->isPresenting();
    const SlideObject *selected = m_editor->selectedObject();

    m_undoAction->setEnabled(m_editor->canUndo());
    m_undoAction->setText(m_editor->canUndo()
                              ? QStringLiteral("Undo %1").arg(m_editor->undoLabel())
                              : QStringLiteral("Undo"));
    m_redoAction->setEnabled(m_editor->canRedo());
    m_redoAction->setText(m_editor->canRedo()
                              ? QStringLiteral("Redo %1").arg(m_editor->redoLabel())
                              : QStringLiteral("Redo"));
    m_saveAction->setEnabled(hasDeck);
    m_exportAction->setEnabled(hasDeck);
    m_newSlideAction->setEnabled(hasDeck);
    m_duplicateObjectAction->setEnabled(selected != nullptr);
    m_deleteObjectAction->setEnabled(selected != nullptr);
    m_editTextAction->setEnabled(selected && selected->isText());
    m_fitAction->setEnabled(hasDeck);
    m_zoomInAction->setEnabled(hasDeck);
    m_zoomOutAction->setEnabled(hasDeck);
    m_nextAction->setEnabled(presenting);
    m_previousAction->setEnabled(presenting);
    m_blackoutAction->setEnabled(presenting);
    m_logoAction->setEnabled(presenting);
    m_presentAction->setEnabled(hasDeck);
    m_presentAction->setText(presenting ? QStringLiteral("Stop presenting")
                                        : QStringLiteral("Start presenting"));
    m_presentAction->setIcon(ui::icon(presenting ? Glyph::Stop : Glyph::Play,
                                      mxu::ui::palette().textMuted, 16));

    {
        QSignalBlocker blocker(m_blackoutAction);
        m_blackoutAction->setChecked(m_editor->blackout());
    }
    {
        QSignalBlocker blocker(m_logoAction);
        m_logoAction->setChecked(m_editor->logoVisible());
    }
    {
        QSignalBlocker blocker(m_stageAction);
        m_stageAction->setChecked(m_stageEnabled);
    }

    if (m_timerChip) {
        m_timerChip->setRemainingSeconds(m_editor->timerRemainingSeconds());
        m_timerChip->setRunning(m_editor->timerRunning());
    }
    m_timerToggleAction->setText(m_editor->timerRunning() ? QStringLiteral("Pause timer")
                                                          : QStringLiteral("Start timer"));
    m_timerToggleAction->setIcon(ui::icon(m_editor->timerRunning() ? Glyph::Pause : Glyph::Play,
                                          mxu::ui::palette().textMuted, 16));

    m_saveState->setText(m_editor->dirty() ? QStringLiteral("Unsaved")
                                           : QStringLiteral("Saved"));
    m_saveState->setStyleSheet(m_editor->dirty()
                                   ? QStringLiteral("color: %1;").arg(mxu::ui::palette().warning.name())
                                   : QStringLiteral("color: %1;").arg(mxu::ui::palette().textFaint.name()));

    m_liveButton->setEnabled(hasDeck);
    m_liveButton->setText(presenting ? QStringLiteral("Stop") : QStringLiteral("Go live"));
    m_liveButton->setIcon(ui::icon(presenting ? Glyph::Stop : Glyph::Play, Qt::white, 15));
    m_liveButton->setToolTip(presenting ? QStringLiteral("Stop presenting (Esc)")
                                        : QStringLiteral("Start presenting (F5)"));

    const bool wasSyncing = m_syncingMode;
    m_syncingMode = true;
    m_modeSwitch->setCurrentValue(presenting ? QStringLiteral("present")
                                             : QStringLiteral("edit"));
    m_syncingMode = wasSyncing;

    if (m_outputButton)
        m_outputButton->setToolTip(QStringLiteral("Program output: %1")
                                       .arg(m_outputScreenName.isEmpty()
                                                ? QStringLiteral("Windowed preview")
                                                : m_outputScreenName));
}

void MainWindow::updateStatus()
{
    if (!m_statusSlide || !m_editor)
        return;
    if (!m_editor->hasPresentation()) {
        m_statusSlide->setText(QStringLiteral("No presentation"));
    } else {
        const int count = m_editor->presentation().slides.size();
        const int index = m_editor->isPresenting() && m_editor->liveIndex() >= 0
            ? m_editor->liveIndex()
            : m_editor->currentIndex();
        m_statusSlide->setText(
            QStringLiteral("%1 · Slide %2 / %3")
                .arg(m_editor->isPresenting() ? QStringLiteral("LIVE")
                                              : QStringLiteral("Edit"))
                .arg(index + 1)
                .arg(count));
    }
    m_statusInfo->setText(QStringLiteral("Qt %1 · Windows").arg(qVersion()));
}

void MainWindow::updatePresentationUi()
{
    if (!m_editor)
        return;
    const bool presenting = m_editor->isPresenting();
    if (presenting) {
        m_libraryPanel->hide();
        m_inspector->hide();
        m_stack->setCurrentWidget(m_presenter);
        m_presenter->refresh();
    } else {
        m_libraryPanel->show();
        m_inspector->show();
        m_stack->setCurrentWidget(m_editorPage);
        if (!m_editSplitterSizes.isEmpty()) {
            m_splitter->setSizes(m_editSplitterSizes);
            m_editSplitterSizes.clear();
        }
    }
    updateActions();
    updateStatus();
}

void MainWindow::newPresentation()
{
    bool ok = false;
    const QString name = QInputDialog::getText(this, QStringLiteral("New presentation"),
                                               QStringLiteral("Presentation name:"),
                                               QLineEdit::Normal, QStringLiteral("Untitled"),
                                               &ok);
    if (!ok)
        return;
    m_editor->createPresentation(name.trimmed().isEmpty() ? QStringLiteral("Untitled")
                                                          : name);
    statusBar()->showMessage(QStringLiteral("Presentation created"), 2500);
}

void MainWindow::importPresentation()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Import presentation"), QString(),
        QStringLiteral("MxU Slides presentation (*.mxuslides);;All files (*)"));
    if (path.isEmpty())
        return;
    QString error;
    if (!m_editor->importBundle(path, &error)) {
        QMessageBox::warning(this, QStringLiteral("Import presentation"), error);
        return;
    }
    statusBar()->showMessage(QStringLiteral("Imported %1").arg(QFileInfo(path).fileName()), 3000);
}

void MainWindow::openBundlePath(const QString &path)
{
    if (path.isEmpty())
        return;
    QString error;
    if (!m_editor->importBundle(path, &error))
        QMessageBox::warning(this, QStringLiteral("Open presentation"), error);
    else
        statusBar()->showMessage(QStringLiteral("Opened %1").arg(QFileInfo(path).fileName()), 3000);
}

void MainWindow::exportPresentation()
{
    if (!m_editor->hasPresentation())
        return;
    flushPendingWork();
    const QString suggested =
        QDir::homePath() + QLatin1Char('/') + safeFileName(m_editor->presentation().name)
        + QStringLiteral(".mxuslides");
    QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Export presentation"), suggested,
        QStringLiteral("MxU Slides presentation (*.mxuslides)"));
    if (path.isEmpty())
        return;
    if (!path.endsWith(QStringLiteral(".mxuslides"), Qt::CaseInsensitive))
        path += QStringLiteral(".mxuslides");

    QString error;
    if (!m_editor->exportBundle(path, &error)) {
        QMessageBox::warning(this, QStringLiteral("Export presentation"), error);
        return;
    }
    statusBar()->showMessage(QStringLiteral("Exported %1").arg(QFileInfo(path).fileName()), 3000);
}

void MainWindow::savePresentation()
{
    if (!m_editor->hasPresentation())
        return;
    flushPendingWork();
    QString error;
    if (!m_editor->save(&error)) {
        QMessageBox::warning(this, QStringLiteral("Save presentation"), error);
        return;
    }
    statusBar()->showMessage(QStringLiteral("Presentation saved"), 2000);
}

bool MainWindow::applyLibraryRoot(const QString &root)
{
    if (root.isEmpty() || root == m_library->rootPath())
        return true;
    const QString previous = m_library->rootPath();
    m_editor->flushAutosave();
    m_library->setRootPath(root);
    QString error;
    if (!m_library->ensureLayout(&error)) {
        QMessageBox::warning(this, QStringLiteral("Library folder"), error);
        m_library->setRootPath(previous);
        return false;
    }

    m_editor->close();
    m_library->seedStarterContent();
    m_editor->reloadThemes();
    m_editor->reloadMedia();
    m_renderer->clearCache();

    const QVector<LibraryEntry> entries = m_library->entries();
    if (!entries.isEmpty()) {
        QString openError;
        if (!m_editor->open(entries.first().id, &openError))
            statusBar()->showMessage(openError, 4000);
    } else {
        m_editor->createPresentation(QStringLiteral("Welcome"));
    }
    m_settings.setValue(QStringLiteral("library/root"), root);
    updateWindowTitle();
    updateActions();
    updateStatus();
    return true;
}

void MainWindow::openSettings()
{
    if (m_editor->isPresenting())
        stopPresenting();
    flushPendingWork();

    SettingsDialog dialog(m_library->rootPath(), m_outputScreenName, m_stageEnabled,
                          m_stageScreenName, m_editor->timerTotalSeconds(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    if (dialog.libraryRoot().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Settings"),
                             QStringLiteral("Choose a library folder."));
        return;
    }
    if (!applyLibraryRoot(dialog.libraryRoot()))
        return;

    m_outputScreenName = dialog.outputScreen();
    m_stageScreenName = dialog.stageScreen();
    m_stageEnabled = dialog.stageEnabled();
    m_settings.setValue(QStringLiteral("output/screen"), m_outputScreenName);
    m_settings.setValue(QStringLiteral("stage/screen"), m_stageScreenName);
    m_settings.setValue(QStringLiteral("stage/enabled"), m_stageEnabled);

    m_editor->setTimerTotalSeconds(dialog.timerSeconds());
    m_editor->resetTimer();
    m_settings.setValue(QStringLiteral("timer/seconds"), dialog.timerSeconds());

    {
        QSignalBlocker blocker(m_stageAction);
        m_stageAction->setChecked(m_stageEnabled);
    }
    rebuildOutputMenu();

    if (m_program && m_editor->isPresenting()) {
        m_program->setTargetScreen(resolveOutputScreen(m_outputScreenName));
        m_program->setPresenting(true);
    }
    if (m_stageEnabled && m_editor->isPresenting()) {
        ensureStageWindow();
        m_stage->setTargetScreen(resolveStageScreen());
        m_stage->setActive(true);
    } else if (m_stage) {
        m_stage->setActive(false);
    }
    updateActions();
    statusBar()->showMessage(QStringLiteral("Settings saved"), 2500);
}

QScreen *MainWindow::resolveOutputScreen(const QString &name) const
{
    if (name.isEmpty())
        return nullptr;
    const QList<QScreen *> screens = QGuiApplication::screens();
    if (name == QLatin1String("auto")) {
        const QScreen *primary = QGuiApplication::primaryScreen();
        for (QScreen *screen : screens) {
            if (screen != primary)
                return screen;
        }
        return nullptr;
    }
    for (QScreen *screen : screens) {
        if (screen->name() == name)
            return screen;
    }
    return nullptr;
}

QScreen *MainWindow::resolveStageScreen() const
{
    const QList<QScreen *> screens = QGuiApplication::screens();
    if (!m_stageScreenName.isEmpty()) {
        for (QScreen *screen : screens) {
            if (screen->name() == m_stageScreenName)
                return screen;
        }
        return nullptr;
    }
    const QScreen *primary = QGuiApplication::primaryScreen();
    for (QScreen *screen : screens) {
        if (screen != primary)
            return screen;
    }
    return nullptr;
}

void MainWindow::rebuildOutputMenu()
{
    if (!m_outputMenu)
        return;
    delete m_outputGroup;
    m_outputGroup = new QActionGroup(this);
    m_outputGroup->setExclusive(true);
    m_outputMenu->clear();

    auto addDisplay = [this](const QString &label, const QString &name) {
        QAction *action = m_outputMenu->addAction(label);
        action->setCheckable(true);
        action->setChecked(m_outputScreenName == name);
        m_outputGroup->addAction(action);
        connect(action, &QAction::triggered, this, [this, name] { chooseOutputScreen(name); });
    };

    addDisplay(QStringLiteral("Windowed preview"), QString());
    addDisplay(QStringLiteral("Automatic external display"), QStringLiteral("auto"));
    m_outputMenu->addSeparator();
    for (QScreen *screen : QGuiApplication::screens()) {
        QString label = screen->name();
        if (label.isEmpty())
            label = QStringLiteral("Display %1").arg(screen->geometry().width());
        if (!screen->name().isEmpty())
            label += QStringLiteral("  ·  %1×%2")
                         .arg(screen->geometry().width())
                         .arg(screen->geometry().height());
        addDisplay(label, screen->name());
    }
    m_outputMenu->addSeparator();
    m_outputMenu->addAction(m_outputSettingsAction);
}

void MainWindow::chooseOutputScreen(const QString &name)
{
    m_outputScreenName = name;
    m_settings.setValue(QStringLiteral("output/screen"), name);
    if (m_program && m_editor->isPresenting()) {
        m_program->setTargetScreen(resolveOutputScreen(name));
        m_program->setPresenting(true);
    }
    QTimer::singleShot(0, this, &MainWindow::rebuildOutputMenu);
    updateActions();
}

void MainWindow::ensureProgramWindow()
{
    if (m_program)
        return;
    m_program = new ProgramWindow(m_editor, m_renderer);
    connect(m_program, &ProgramWindow::stopRequested, this, &MainWindow::stopPresenting);
}

void MainWindow::ensureStageWindow()
{
    if (m_stage)
        return;
    m_stage = new StageWindow(m_editor, m_renderer);
}

void MainWindow::startPresenting()
{
    if (!m_editor->hasPresentation() || m_editor->presentation().slides.isEmpty())
        return;
    flushPendingWork();
    if (!m_editor->isPresenting())
        m_editSplitterSizes = m_splitter->sizes();

    m_editor->startPresenting();
    ensureProgramWindow();
    m_program->setTargetScreen(resolveOutputScreen(m_outputScreenName));
    m_program->setPresenting(true);

    if (m_stageEnabled) {
        ensureStageWindow();
        m_stage->setTargetScreen(resolveStageScreen());
        m_stage->setActive(true);
    } else if (m_stage) {
        m_stage->setActive(false);
    }

    updatePresentationUi();
    m_presenter->refresh();
    statusBar()->showMessage(QStringLiteral("Presenting — Esc to stop"), 2500);
}

void MainWindow::stopPresenting()
{
    if (m_program) {
        m_program->setPresenting(false);
        m_program->hide();
    }
    if (m_stage) {
        m_stage->setActive(false);
        m_stage->hide();
    }
    if (m_editor->isPresenting())
        m_editor->stopPresenting();
    updatePresentationUi();
    updateActions();
    updateStatus();
}

void MainWindow::togglePresenting()
{
    if (m_editor->isPresenting())
        stopPresenting();
    else
        startPresenting();
}

void MainWindow::toggleStageDisplay(bool enabled)
{
    m_stageEnabled = enabled;
    m_settings.setValue(QStringLiteral("stage/enabled"), enabled);
    if (enabled && m_editor->isPresenting()) {
        ensureStageWindow();
        m_stage->setTargetScreen(resolveStageScreen());
        m_stage->setActive(true);
    } else if (m_stage) {
        m_stage->setActive(false);
        if (!enabled)
            m_stage->hide();
    }
    updateActions();
}

void MainWindow::editTextObject(const QString &objectId)
{
    if (!m_editor->hasPresentation())
        return;
    const int slideIndex = m_editor->currentIndex();
    if (slideIndex < 0 || slideIndex >= m_editor->presentation().slides.size())
        return;
    const Slide &slide = m_editor->presentation().slides.at(slideIndex);
    const int objectIndex = slide.indexOfObject(objectId);
    if (objectIndex < 0)
        return;
    const SlideObject object = slide.objects.at(objectIndex);
    if (!object.isText())
        return;
    m_editor->selectObject(objectId);

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Edit text — %1").arg(object.name));
    dialog.resize(620, 360);
    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(16, 14, 16, 12);
    layout->setSpacing(9);
    layout->addWidget(makeLabel(QStringLiteral("TEXT"), QStringLiteral("SectionTitle")));
    auto *edit = new QPlainTextEdit;
    edit->setPlainText(object.text);
    edit->setTabChangesFocus(true);
    layout->addWidget(edit, 1);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    if (auto *apply = buttons->button(QDialogButtonBox::Save))
        apply->setText(QStringLiteral("Apply"));
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    edit->setFocus();

    if (dialog.exec() != QDialog::Accepted)
        return;
    const QString text = edit->toPlainText();
    m_editor->commit(QStringLiteral("Edit text"),
                     [slideIndex, objectId, text](Presentation &deck) {
                         if (slideIndex < 0 || slideIndex >= deck.slides.size())
                             return;
                         for (SlideObject &candidate : deck.slides[slideIndex].objects) {
                             if (candidate.id == objectId) {
                                 candidate.text = text;
                                 break;
                             }
                         }
                     });
}

void MainWindow::configureTimer()
{
    bool ok = false;
    const int currentMinutes = std::max(1, m_editor->timerTotalSeconds() / 60);
    const int minutes = QInputDialog::getInt(this, QStringLiteral("Presentation timer"),
                                              QStringLiteral("Duration in minutes:"),
                                              currentMinutes, 1, 180, 1, &ok);
    if (!ok)
        return;
    m_editor->setTimerTotalSeconds(minutes * 60);
    m_editor->resetTimer();
    m_settings.setValue(QStringLiteral("timer/seconds"), minutes * 60);
    statusBar()->showMessage(QStringLiteral("Timer set to %1 minutes").arg(minutes), 2000);
}

void MainWindow::goToSlideNumber(int number)
{
    if (!m_editor->hasPresentation())
        return;
    if (number < 1 || number > m_editor->presentation().slides.size())
        return;
    m_editor->goToSlideNumber(number);
}

void MainWindow::showShortcuts()
{
    QMessageBox box(this);
    box.setWindowTitle(QStringLiteral("Keyboard shortcuts"));
    box.setTextFormat(Qt::RichText);
    box.setText(QStringLiteral(
        "<b>Editing</b><br>"
        "Ctrl+N — New presentation<br>"
        "Ctrl+Shift+N — New slide<br>"
        "Ctrl+S — Save · Ctrl+O — Import bundle<br>"
        "Ctrl+Z / Ctrl+Shift+Z — Undo / Redo<br>"
        "Ctrl+D — Duplicate object · F2 — Edit text<br>"
        "Delete — Delete selected object<br>"
        "Arrow keys — Nudge object (Shift = 10 px)<br>"
        "Ctrl+0 — Fit slide · Ctrl+wheel — Zoom<br><br>"
        "<b>Presenting</b><br>"
        "F5 — Start · Esc — Stop<br>"
        "Space / → / Page Down — Next slide<br>"
        "← / Page Up — Previous slide<br>"
        "Home / End — First / last slide<br>"
        "B — Blackout · L — Logo screen<br>"
        "Type a slide number then Enter — Jump to slide"));
    box.exec();
}

void MainWindow::showAbout()
{
    QMessageBox::about(
        this, QStringLiteral("About MxU Slides"),
        QStringLiteral(
            "<b>MxU Slides</b> %1<br>"
            "A native Windows presentation editor built with C++20 and Qt 6.12.<br><br>"
            "Documents use the shared MxU <code>.mxuslides</code> schema with macOS. "
            "Unknown macOS-only fields are preserved on export.<br><br>"
            "Video playback, audio, streaming, NDI, DeckLink and the macOS Local API "
            "are not implemented in this build.")
            .arg(QCoreApplication::applicationVersion()));
}

void MainWindow::flushPendingWork()
{
    if (m_inspector)
        m_inspector->flushPendingEdits();
    if (m_editor)
        m_editor->flushAutosave();
}

bool MainWindow::handlePresentationKey(QKeyEvent *event)
{
    const int pressed = event->key();
    const Qt::KeyboardModifiers modifiers = event->modifiers();
    const bool plain = modifiers == Qt::NoModifier || modifiers == Qt::KeypadModifier;

    if (plain && pressed >= Qt::Key_0 && pressed <= Qt::Key_9) {
        if (m_goToBuffer.size() >= 3)
            m_goToBuffer.remove(0, 1);
        m_goToBuffer += QString::number(pressed - Qt::Key_0);
        m_goToTimer->start();
        statusBar()->showMessage(
            QStringLiteral("Go to slide %1 — press Enter").arg(m_goToBuffer), 1600);
        return true;
    }
    if (pressed == Qt::Key_Backspace && !m_goToBuffer.isEmpty()) {
        m_goToBuffer.chop(1);
        m_goToTimer->start();
        statusBar()->showMessage(m_goToBuffer.isEmpty()
                                     ? QStringLiteral("Jump cancelled")
                                     : QStringLiteral("Go to slide %1 — press Enter")
                                           .arg(m_goToBuffer),
                                 1600);
        return true;
    }
    if (pressed == Qt::Key_Escape) {
        m_goToBuffer.clear();
        stopPresenting();
        return true;
    }
    if (pressed == Qt::Key_Right || pressed == Qt::Key_Space
        || pressed == Qt::Key_PageDown || pressed == Qt::Key_Return
        || pressed == Qt::Key_Enter) {
        if (!m_goToBuffer.isEmpty()) {
            const int number = m_goToBuffer.toInt();
            m_goToBuffer.clear();
            m_goToTimer->stop();
            statusBar()->clearMessage();
            goToSlideNumber(number);
        } else {
            m_editor->next();
        }
        return true;
    }
    if (pressed == Qt::Key_Left || pressed == Qt::Key_PageUp) {
        m_editor->previous();
        return true;
    }
    if (pressed == Qt::Key_Home) {
        goToSlideNumber(1);
        return true;
    }
    if (pressed == Qt::Key_End) {
        goToSlideNumber(static_cast<int>(m_editor->presentation().slides.size()));
        return true;
    }
    if (plain && pressed == Qt::Key_B) {
        m_editor->setBlackout(!m_editor->blackout());
        return true;
    }
    if (plain && pressed == Qt::Key_L) {
        m_editor->setLogoVisible(!m_editor->logoVisible());
        return true;
    }
    return false;
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() != QEvent::KeyPress)
        return QMainWindow::eventFilter(watched, event);
    if (QApplication::activeModalWidget() || QApplication::activePopupWidget())
        return false;

    auto *keyEvent = static_cast<QKeyEvent *>(event);
    if (keyEvent->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))
        return false;

    QWidget *focus = QApplication::focusWidget();
    if (focus
        && (qobject_cast<QLineEdit *>(focus) || qobject_cast<QTextEdit *>(focus)
            || qobject_cast<QPlainTextEdit *>(focus) || qobject_cast<QAbstractSpinBox *>(focus)
            || qobject_cast<QComboBox *>(focus))) {
        return false;
    }

    QWidget *active = QApplication::activeWindow();
    const bool ours = active == this || (m_program && active == m_program)
        || (m_stage && active == m_stage);
    if (!ours)
        return false;

    if (!m_editor->isPresenting()) {
        if (keyEvent->key() == Qt::Key_F5) {
            startPresenting();
            return true;
        }
        return false;
    }
    return handlePresentationKey(keyEvent);
}

void MainWindow::saveSession()
{
    m_settings.setValue(QStringLiteral("window/geometry"), saveGeometry());
    m_settings.setValue(QStringLiteral("window/splitter"), m_splitter->saveState());
    m_settings.setValue(QStringLiteral("output/screen"), m_outputScreenName);
    m_settings.setValue(QStringLiteral("stage/enabled"), m_stageEnabled);
    m_settings.setValue(QStringLiteral("stage/screen"), m_stageScreenName);
    m_settings.setValue(QStringLiteral("timer/seconds"), m_editor->timerTotalSeconds());
    m_settings.sync();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    flushPendingWork();
    if (m_editor->dirty()) {
        QString error;
        m_editor->save(&error);
    }
    if (m_editor->isPresenting())
        stopPresenting();
    if (m_program) {
        m_program->setPresenting(false);
        m_program->close();
    }
    if (m_stage) {
        m_stage->setActive(false);
        m_stage->close();
    }
    saveSession();
    qApp->removeEventFilter(this);
    QMainWindow::closeEvent(event);
}

} // namespace mxu::ui
