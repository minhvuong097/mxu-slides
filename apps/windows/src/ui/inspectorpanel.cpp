#include "ui/inspectorpanel.h"

#include "core/editor.h"
#include "core/library.h"
#include "ui/icons.h"
#include "ui/theme.h"
#include "ui/widgets.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFrame>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace mxu::ui {
namespace {

QString kindLabel(const SlideObject &object)
{
    if (object.isText())
        return QStringLiteral("Text");
    if (object.isShape()) {
        if (object.shapeKind == QLatin1String("ellipse"))
            return QStringLiteral("Shape · Ellipse");
        if (object.shapeKind == QLatin1String("roundedRectangle"))
            return QStringLiteral("Shape · Rounded rectangle");
        return QStringLiteral("Shape · Rectangle");
    }
    if (object.isMedia())
        return QStringLiteral("Media · Image");
    return QStringLiteral("Object");
}

QRectF displayFrame(const SlideObject &object, const Presentation &deck)
{
    if (object.hasFrame)
        return object.frame;
    return QRectF(deck.canvasWidth * 0.06, deck.canvasHeight * 0.16,
                  deck.canvasWidth * 0.88, deck.canvasHeight * 0.66);
}

QFormLayout *newForm()
{
    auto *form = new QFormLayout;
    form->setContentsMargins(0, 0, 0, 0);
    form->setHorizontalSpacing(10);
    form->setVerticalSpacing(7);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->setRowWrapPolicy(QFormLayout::DontWrapRows);
    return form;
}

QDoubleSpinBox *numberField(double minimum, double maximum, double step, int decimals = 0,
                            const QString &suffix = QString())
{
    auto *spin = new QDoubleSpinBox;
    spin->setRange(minimum, maximum);
    spin->setSingleStep(step);
    spin->setDecimals(decimals);
    spin->setSuffix(suffix);
    spin->setKeyboardTracking(false);
    spin->setAlignment(Qt::AlignRight);
    return spin;
}

QWidget *row(std::initializer_list<QWidget *> widgets)
{
    auto *container = new QWidget;
    auto *layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);
    for (QWidget *widget : widgets)
        layout->addWidget(widget);
    return container;
}

QScrollArea *scrollPage(QWidget *content)
{
    auto *area = new QScrollArea;
    area->setWidgetResizable(true);
    area->setFrameShape(QFrame::NoFrame);
    area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    area->setWidget(content);
    return area;
}

QLabel *hintLabel(const QString &text)
{
    auto *label = makeLabel(text, QStringLiteral("Faint"));
    label->setWordWrap(true);
    return label;
}

struct UpdateGuard {
    explicit UpdateGuard(bool &flag)
        : m_flag(flag)
        , m_previous(flag)
    {
        m_flag = true;
    }
    ~UpdateGuard() { m_flag = m_previous; }
    bool &m_flag;
    bool m_previous;
};

class ThemeDialog : public QDialog {
public:
    ThemeDialog(const Theme &theme, QWidget *parent = nullptr)
        : QDialog(parent)
        , m_theme(theme)
    {
        setWindowTitle(QStringLiteral("Edit theme"));
        setMinimumWidth(390);
        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(18, 18, 18, 14);
        layout->setSpacing(12);

        auto *title = makeLabel(QStringLiteral("Theme · %1").arg(theme.name), QStringLiteral("Title"));
        layout->addWidget(title);
        layout->addWidget(hintLabel(
            QStringLiteral("Theme changes apply to every presentation using this theme.")));

        auto *form = newForm();
        m_font = new QComboBox;
        const QStringList families = QFontDatabase::families();
        m_font->addItems(families);
        const int fontIndex = m_font->findText(theme.fontFamily, Qt::MatchFixedString);
        if (fontIndex >= 0)
            m_font->setCurrentIndex(fontIndex);
        form->addRow(QStringLiteral("Font"), m_font);

        m_size = numberField(8, 500, 2, 0, QStringLiteral(" px"));
        m_size->setValue(theme.fontSize);
        form->addRow(QStringLiteral("Base size"), m_size);

        m_textColor = new ColorButton;
        m_textColor->setColor(theme.textColor());
        form->addRow(QStringLiteral("Text"), m_textColor);

        m_background = new ColorButton;
        m_background->setColor(theme.backgroundColor());
        form->addRow(QStringLiteral("Background"), m_background);
        layout->addLayout(form);

        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        layout->addWidget(buttons);
    }

    Theme result() const
    {
        Theme theme = m_theme;
        theme.fontFamily = m_font->currentText();
        theme.fontSize = m_size->value();
        theme.textColorHex = colorToHex(m_textColor->color());
        theme.backgroundColorHex = colorToHex(m_background->color());
        return theme;
    }

private:
    Theme m_theme;
    QComboBox *m_font = nullptr;
    QDoubleSpinBox *m_size = nullptr;
    ColorButton *m_textColor = nullptr;
    ColorButton *m_background = nullptr;
};

} // namespace

InspectorPanel::InspectorPanel(Editor *editor, SlideRenderer *renderer, QWidget *parent)
    : QWidget(parent)
    , m_editor(editor)
    , m_renderer(renderer)
{
    setObjectName(QStringLiteral("InspectorPanel"));
    setMinimumWidth(286);
    setMaximumWidth(380);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 14, 12, 10);
    layout->setSpacing(8);

    auto *heading = new QHBoxLayout();
    heading->addWidget(makeLabel(QStringLiteral("INSPECTOR"), QStringLiteral("SectionTitle")));
    heading->addStretch();
    layout->addLayout(heading);

    m_tabs = new QTabWidget(this);
    m_tabs->setDocumentMode(true);
    m_tabs->setUsesScrollButtons(false);
    layout->addWidget(m_tabs, 1);

    buildSlidePage();
    buildObjectPage();
    buildDeckPage();

    m_textTimer = new QTimer(this);
    m_textTimer->setSingleShot(true);
    m_textTimer->setInterval(320);
    m_notesTimer = new QTimer(this);
    m_notesTimer->setSingleShot(true);
    m_notesTimer->setInterval(320);
    connect(m_textTimer, &QTimer::timeout, this, &InspectorPanel::applyPendingText);
    connect(m_notesTimer, &QTimer::timeout, this, &InspectorPanel::applyPendingNotes);

    connect(editor, &Editor::presentationChanged, this, &InspectorPanel::refresh);
    connect(editor, &Editor::presentationEdited, this, &InspectorPanel::refresh);
    connect(editor, &Editor::slideIndexChanged, this, [this] {
        flushPendingEdits();
        refresh();
    });
    connect(editor, &Editor::selectionChanged, this, [this] {
        flushPendingEdits();
        refresh();
    });
    connect(editor, &Editor::libraryChanged, this, &InspectorPanel::updateDeckFields);

    updateFontFamilies();
    refresh();
}

void InspectorPanel::buildSlidePage()
{
    auto *content = new QWidget;
    auto *outer = new QVBoxLayout(content);
    outer->setContentsMargins(2, 8, 2, 8);
    outer->setSpacing(10);

    auto *slideCard = new SectionCard(QStringLiteral("SLIDE"), content);
    auto *form = newForm();
    m_slideName = new QLineEdit;
    m_slideName->setPlaceholderText(QStringLiteral("Slide name"));
    form->addRow(QStringLiteral("Name"), m_slideName);

    m_slideBackground = new ColorButton;
    m_slideBackgroundReset = new QPushButton(QStringLiteral("Use theme"));
    m_slideBackgroundReset->setObjectName(QStringLiteral("Ghost"));
    form->addRow(QStringLiteral("Background"),
                 row({m_slideBackground, m_slideBackgroundReset}));
    slideCard->body()->addLayout(form);
    outer->addWidget(slideCard);

    auto *notesCard = new SectionCard(QStringLiteral("SPEAKER NOTES"), content);
    m_notes = new QPlainTextEdit;
    m_notes->setPlaceholderText(QStringLiteral("Notes for the presenter and stage display"));
    m_notes->setFixedHeight(126);
    notesCard->body()->addWidget(m_notes);
    outer->addWidget(notesCard);

    auto *transitionCard = new SectionCard(QStringLiteral("TRANSITION"), content);
    m_transitionEnabled = new QCheckBox(QStringLiteral("Use a custom transition for this slide"));
    transitionCard->body()->addWidget(m_transitionEnabled);
    auto *transitionForm = newForm();
    m_transitionKind = new QComboBox;
    m_transitionKind->addItem(QStringLiteral("Dissolve"), QStringLiteral("dissolve"));
    m_transitionKind->addItem(QStringLiteral("Fade to black"), QStringLiteral("fadeBlack"));
    m_transitionKind->addItem(QStringLiteral("Cut"), QStringLiteral("cut"));
    transitionForm->addRow(QStringLiteral("Style"), m_transitionKind);
    m_transitionDuration = numberField(0.1, 3.0, 0.1, 1, QStringLiteral(" s"));
    transitionForm->addRow(QStringLiteral("Duration"), m_transitionDuration);
    transitionCard->body()->addLayout(transitionForm);
    outer->addWidget(transitionCard);

    m_deleteSlide = new QPushButton(QStringLiteral("Delete slide"));
    m_deleteSlide->setObjectName(QStringLiteral("Danger"));
    outer->addWidget(m_deleteSlide);
    outer->addStretch();

    m_tabs->addTab(scrollPage(content), QStringLiteral("Slide"));

    connect(m_slideName, &QLineEdit::editingFinished, this, [this] {
        if (m_updating)
            return;
        const QString name = m_slideName->text().trimmed();
        if (name.isEmpty()) {
            refresh();
            return;
        }
        mutateSlide(QStringLiteral("Rename slide"), [name](Slide &slide) { slide.name = name; });
    });
    connect(m_slideBackground, &ColorButton::colorChanged, this, [this](const QColor &color) {
        if (m_updating)
            return;
        mutateSlide(QStringLiteral("Change background"),
                    [color](Slide &slide) { slide.backgroundFill = Fill::solid(color); });
    });
    connect(m_slideBackgroundReset, &QPushButton::clicked, this,
            &InspectorPanel::resetSlideBackground);
    connect(m_notes, &QPlainTextEdit::textChanged, this, [this] {
        if (!m_updating)
            scheduleNotesEdit();
    });
    connect(m_transitionEnabled, &QCheckBox::toggled, this, [this](bool enabled) {
        if (m_updating)
            return;
        m_transitionKind->setEnabled(enabled);
        m_transitionDuration->setEnabled(enabled);
        mutateSlide(QStringLiteral("Change transition"),
                    [enabled](Slide &slide) { slide.transition.enabled = enabled; });
    });
    connect(m_transitionKind, QOverload<int>::of(&QComboBox::activated), this, [this](int index) {
        if (m_updating)
            return;
        const QString kind = m_transitionKind->itemData(index).toString();
        const bool enabled = kind != QLatin1String("cut");
        {
            QSignalBlocker blocker(m_transitionEnabled);
            m_transitionEnabled->setChecked(enabled);
        }
        m_transitionKind->setEnabled(enabled);
        m_transitionDuration->setEnabled(enabled);
        mutateSlide(QStringLiteral("Change transition"), [kind, enabled](Slide &slide) {
            slide.transition.kind = kind;
            slide.transition.enabled = enabled;
        });
    });
    connect(m_transitionDuration, &QDoubleSpinBox::editingFinished, this, [this] {
        if (m_updating)
            return;
        const double duration = m_transitionDuration->value();
        mutateSlide(QStringLiteral("Change transition duration"),
                    [duration](Slide &slide) { slide.transition.durationSeconds = duration; });
    });
    connect(m_deleteSlide, &QPushButton::clicked, m_editor, &Editor::deleteCurrentSlide);
}

void InspectorPanel::buildObjectPage()
{
    m_objectStack = new QStackedWidget;

    auto *empty = new QWidget;
    auto *emptyLayout = new QVBoxLayout(empty);
    emptyLayout->setContentsMargins(18, 24, 18, 24);
    emptyLayout->addStretch();
    auto *mark = new QLabel;
    mark->setPixmap(appMarkPixmap(44));
    mark->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(mark);
    auto *emptyTitle = makeLabel(QStringLiteral("Nothing selected"), QStringLiteral("Muted"));
    emptyTitle->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(emptyTitle);
    auto *emptyText = hintLabel(
        QStringLiteral("Select an object on the slide to edit its text, fill, position, and effects."));
    emptyText->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(emptyText);
    emptyLayout->addStretch();
    m_objectStack->addWidget(empty);

    auto *content = new QWidget;
    auto *outer = new QVBoxLayout(content);
    outer->setContentsMargins(2, 8, 2, 8);
    outer->setSpacing(10);

    auto *objectCard = new SectionCard(QStringLiteral("OBJECT"), content);
    auto *objectForm = newForm();
    m_objectType = makeLabel(QString(), QStringLiteral("Faint"));
    objectForm->addRow(QStringLiteral("Type"), m_objectType);
    m_objectName = new QLineEdit;
    m_objectName->setPlaceholderText(QStringLiteral("Object name"));
    objectForm->addRow(QStringLiteral("Name"), m_objectName);
    objectCard->body()->addLayout(objectForm);
    outer->addWidget(objectCard);

    m_textSection = new SectionCard(QStringLiteral("TEXT"), content);
    auto *textCard = static_cast<SectionCard *>(m_textSection);
    m_objectText = new QPlainTextEdit;
    m_objectText->setPlaceholderText(QStringLiteral("Text content"));
    m_objectText->setFixedHeight(92);
    m_objectText->setTabChangesFocus(true);
    textCard->body()->addWidget(m_objectText);
    auto *textForm = newForm();
    m_fontFamily = new QComboBox;
    m_fontFamily->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_fontFamily->setMinimumContentsLength(10);
    textForm->addRow(QStringLiteral("Font"), m_fontFamily);
    m_fontSize = numberField(0, 1000, 2, 0, QStringLiteral(" px"));
    m_fontSize->setSpecialValueText(QStringLiteral("Theme"));
    textForm->addRow(QStringLiteral("Size"), m_fontSize);
    m_textColorSet = new QCheckBox(QStringLiteral("Custom"));
    m_textColor = new ColorButton;
    textForm->addRow(QStringLiteral("Color"), row({m_textColorSet, m_textColor}));
    m_hAlignment = new SegmentedControl;
    m_hAlignment->addItem(QStringLiteral("L"), QStringLiteral("left"));
    m_hAlignment->addItem(QStringLiteral("C"), QStringLiteral("center"));
    m_hAlignment->addItem(QStringLiteral("R"), QStringLiteral("right"));
    textForm->addRow(QStringLiteral("Align"), m_hAlignment);
    m_vAlignment = new SegmentedControl;
    m_vAlignment->addItem(QStringLiteral("T"), QStringLiteral("top"));
    m_vAlignment->addItem(QStringLiteral("M"), QStringLiteral("middle"));
    m_vAlignment->addItem(QStringLiteral("B"), QStringLiteral("bottom"));
    textForm->addRow(QStringLiteral("Vertical"), m_vAlignment);
    m_autoShrink = new QCheckBox(QStringLiteral("Shrink text to fit its frame"));
    textForm->addRow(QString(), m_autoShrink);
    textCard->body()->addLayout(textForm);
    outer->addWidget(m_textSection);

    m_shapeSection = new SectionCard(QStringLiteral("SHAPE"), content);
    auto *shapeCard = static_cast<SectionCard *>(m_shapeSection);
    auto *shapeForm = newForm();
    m_shapeKind = new QComboBox;
    m_shapeKind->addItem(QStringLiteral("Rectangle"), QStringLiteral("rectangle"));
    m_shapeKind->addItem(QStringLiteral("Rounded rectangle"), QStringLiteral("roundedRectangle"));
    m_shapeKind->addItem(QStringLiteral("Ellipse"), QStringLiteral("ellipse"));
    shapeForm->addRow(QStringLiteral("Shape"), m_shapeKind);
    m_cornerRadius = numberField(0, 1000, 2, 0, QStringLiteral(" px"));
    shapeForm->addRow(QStringLiteral("Corners"), m_cornerRadius);
    shapeCard->body()->addLayout(shapeForm);
    outer->addWidget(m_shapeSection);

    m_fillSection = new SectionCard(QStringLiteral("FILL"), content);
    auto *fillCard = static_cast<SectionCard *>(m_fillSection);
    m_fillForm = newForm();
    m_fillKind = new QComboBox;
    m_fillKind->addItem(QStringLiteral("None"), QStringLiteral("none"));
    m_fillKind->addItem(QStringLiteral("Solid color"), QStringLiteral("solid"));
    m_fillKind->addItem(QStringLiteral("Linear gradient"), QStringLiteral("linearGradient"));
    m_fillForm->addRow(QStringLiteral("Style"), m_fillKind);
    m_fillColor = new ColorButton;
    m_fillColorRow = row({m_fillColor});
    m_fillForm->addRow(QStringLiteral("Color"), m_fillColorRow);
    m_gradientColor = new ColorButton;
    m_gradientAngle = numberField(0, 360, 15, 0, QStringLiteral("°"));
    m_gradientRow = row({m_gradientColor, m_gradientAngle});
    m_fillForm->addRow(QStringLiteral("Gradient"), m_gradientRow);
    fillCard->body()->addLayout(m_fillForm);
    outer->addWidget(m_fillSection);

    m_mediaSection = new SectionCard(QStringLiteral("MEDIA"), content);
    auto *mediaCard = static_cast<SectionCard *>(m_mediaSection);
    m_mediaName = makeLabel(QString(), QStringLiteral("Faint"));
    m_mediaName->setWordWrap(true);
    mediaCard->body()->addWidget(m_mediaName);
    auto *choose = new QPushButton(QStringLiteral("Choose image…"));
    auto *clear = new QPushButton(QStringLiteral("Remove"));
    clear->setObjectName(QStringLiteral("Ghost"));
    mediaCard->body()->addWidget(row({choose, clear}));
    auto *mediaForm = newForm();
    m_mediaScale = new QComboBox;
    m_mediaScale->addItem(QStringLiteral("Fill frame"), QStringLiteral("fill"));
    m_mediaScale->addItem(QStringLiteral("Fit inside"), QStringLiteral("fit"));
    m_mediaScale->addItem(QStringLiteral("Stretch"), QStringLiteral("stretch"));
    m_mediaScale->addItem(QStringLiteral("Blurred fill"), QStringLiteral("scaleAndBlur"));
    mediaForm->addRow(QStringLiteral("Scaling"), m_mediaScale);
    mediaCard->body()->addLayout(mediaForm);
    outer->addWidget(m_mediaSection);

    auto *strokeCard = new SectionCard(QStringLiteral("OUTLINE"), content);
    m_strokeEnabled = new QCheckBox(QStringLiteral("Show an outline"));
    strokeCard->body()->addWidget(m_strokeEnabled);
    auto *strokeForm = newForm();
    m_strokeColor = new ColorButton;
    strokeForm->addRow(QStringLiteral("Color"), m_strokeColor);
    m_strokeWidth = numberField(0.0, 100.0, 0.5, 1, QStringLiteral(" px"));
    strokeForm->addRow(QStringLiteral("Width"), m_strokeWidth);
    strokeCard->body()->addLayout(strokeForm);
    outer->addWidget(strokeCard);

    auto *shadowCard = new SectionCard(QStringLiteral("SHADOW"), content);
    m_shadowEnabled = new QCheckBox(QStringLiteral("Drop shadow"));
    shadowCard->body()->addWidget(m_shadowEnabled);
    auto *shadowForm = newForm();
    m_shadowColor = new ColorButton;
    shadowForm->addRow(QStringLiteral("Color"), m_shadowColor);
    m_shadowBlur = numberField(0, 200, 2, 0, QStringLiteral(" px"));
    shadowForm->addRow(QStringLiteral("Blur"), m_shadowBlur);
    m_shadowX = numberField(-500, 500, 2, 0, QStringLiteral(" px"));
    m_shadowY = numberField(-500, 500, 2, 0, QStringLiteral(" px"));
    shadowForm->addRow(QStringLiteral("Offset X"), m_shadowX);
    shadowForm->addRow(QStringLiteral("Offset Y"), m_shadowY);
    shadowCard->body()->addLayout(shadowForm);
    outer->addWidget(shadowCard);

    auto *frameCard = new SectionCard(QStringLiteral("POSITION & SIZE"), content);
    auto *frameForm = newForm();
    m_frameX = numberField(-10000, 10000, 2, 0, QStringLiteral(" px"));
    m_frameY = numberField(-10000, 10000, 2, 0, QStringLiteral(" px"));
    m_frameWidth = numberField(1, 20000, 2, 0, QStringLiteral(" px"));
    m_frameHeight = numberField(1, 20000, 2, 0, QStringLiteral(" px"));
    m_rotation = numberField(-180, 180, 1, 0, QStringLiteral("°"));
    m_opacity = numberField(0, 100, 1, 0, QStringLiteral(" %"));
    frameForm->addRow(QStringLiteral("X"), m_frameX);
    frameForm->addRow(QStringLiteral("Y"), m_frameY);
    frameForm->addRow(QStringLiteral("Width"), m_frameWidth);
    frameForm->addRow(QStringLiteral("Height"), m_frameHeight);
    frameForm->addRow(QStringLiteral("Rotation"), m_rotation);
    frameForm->addRow(QStringLiteral("Opacity"), m_opacity);
    frameCard->body()->addLayout(frameForm);
    outer->addWidget(frameCard);

    auto *arrangeCard = new SectionCard(QStringLiteral("ARRANGE"), content);
    auto *grid = new QGridLayout;
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(6);
    m_duplicateObject = new QPushButton(QStringLiteral("Duplicate"));
    m_forwardObject = new QPushButton(QStringLiteral("Forward"));
    m_backwardObject = new QPushButton(QStringLiteral("Backward"));
    m_deleteObject = new QPushButton(QStringLiteral("Delete"));
    m_deleteObject->setObjectName(QStringLiteral("Danger"));
    grid->addWidget(m_duplicateObject, 0, 0);
    grid->addWidget(m_forwardObject, 0, 1);
    grid->addWidget(m_backwardObject, 1, 0);
    grid->addWidget(m_deleteObject, 1, 1);
    arrangeCard->body()->addLayout(grid);
    outer->addWidget(arrangeCard);
    outer->addStretch();

    m_objectStack->addWidget(scrollPage(content));
    m_tabs->addTab(m_objectStack, QStringLiteral("Object"));

    connect(m_objectName, &QLineEdit::editingFinished, this, [this] {
        if (m_updating)
            return;
        const QString name = m_objectName->text().trimmed();
        if (name.isEmpty()) {
            refresh();
            return;
        }
        mutateObject(QStringLiteral("Rename object"), [name](SlideObject &object) {
            object.name = name;
        });
    });
    connect(m_objectText, &QPlainTextEdit::textChanged, this, [this] {
        if (!m_updating)
            scheduleTextEdit();
    });
    connect(m_fontFamily, QOverload<int>::of(&QComboBox::activated), this, [this](int index) {
        if (m_updating)
            return;
        const QString family = m_fontFamily->itemData(index).toString();
        mutateObject(QStringLiteral("Change font"), [family](SlideObject &object) {
            object.textStyle.fontName = family;
        });
    });
    connect(m_fontSize, &QDoubleSpinBox::editingFinished, this, [this] {
        if (m_updating)
            return;
        const double size = m_fontSize->value();
        mutateObject(QStringLiteral("Change font size"), [size](SlideObject &object) {
            object.textStyle.fontSize = size <= 0.0 ? 0.0 : size;
        });
    });
    connect(m_textColorSet, &QCheckBox::toggled, this, [this](bool custom) {
        if (m_updating)
            return;
        m_textColor->setEnabled(custom);
        const QColor color = m_textColor->color();
        mutateObject(custom ? QStringLiteral("Use custom text color")
                            : QStringLiteral("Use theme text color"),
                    [custom, color](SlideObject &object) {
                        object.textStyle.colorSet = custom;
                        if (custom)
                            object.textStyle.color = color;
                    });
    });
    connect(m_textColor, &ColorButton::colorChanged, this, [this](const QColor &color) {
        if (m_updating || !m_textColorSet->isChecked())
            return;
        mutateObject(QStringLiteral("Change text color"), [color](SlideObject &object) {
            object.textStyle.colorSet = true;
            object.textStyle.color = color;
        });
    });
    connect(m_hAlignment, &SegmentedControl::changed, this, [this](const QString &value) {
        if (m_updating)
            return;
        mutateObject(QStringLiteral("Align text"), [value](SlideObject &object) {
            object.textStyle.horizontalAlignment = value;
        });
    });
    connect(m_vAlignment, &SegmentedControl::changed, this, [this](const QString &value) {
        if (m_updating)
            return;
        mutateObject(QStringLiteral("Align text vertically"), [value](SlideObject &object) {
            object.textStyle.verticalAlignment = value;
        });
    });
    connect(m_autoShrink, &QCheckBox::toggled, this, [this](bool enabled) {
        if (m_updating)
            return;
        mutateObject(QStringLiteral("Change text fitting"), [enabled](SlideObject &object) {
            object.textStyle.autoShrink = enabled;
        });
    });
    connect(m_shapeKind, QOverload<int>::of(&QComboBox::activated), this, [this](int index) {
        if (m_updating)
            return;
        const QString kind = m_shapeKind->itemData(index).toString();
        mutateObject(QStringLiteral("Change shape"), [kind](SlideObject &object) {
            object.shapeKind = kind;
        });
    });
    connect(m_cornerRadius, &QDoubleSpinBox::editingFinished, this, [this] {
        if (m_updating)
            return;
        const double radius = m_cornerRadius->value();
        mutateObject(QStringLiteral("Change corner radius"), [radius](SlideObject &object) {
            object.cornerRadius = radius;
        });
    });
    connect(m_fillKind, QOverload<int>::of(&QComboBox::activated), this, [this](int index) {
        if (m_updating)
            return;
        const QString kind = m_fillKind->itemData(index).toString();
        const QColor themeColor = m_editor->theme().textColor();
        mutateObject(QStringLiteral("Change fill"), [kind, themeColor](SlideObject &object) {
            object.fill.kind = kind;
            if (kind == QLatin1String("solid") && object.fill.color.alpha() == 0)
                object.fill.color = themeColor;
            if (kind == QLatin1String("linearGradient")) {
                const QColor start = object.fill.color.alpha() > 0 ? object.fill.color : themeColor;
                object.fill.color = start;
                if (object.fill.stops.size() < 2) {
                    object.fill.stops = {GradientStop{start, 0.0},
                                         GradientStop{start.darker(170), 1.0}};
                }
            }
        });
        syncFillControls();
    });
    connect(m_fillColor, &ColorButton::colorChanged, this, [this](const QColor &color) {
        if (m_updating)
            return;
        mutateObject(QStringLiteral("Change fill color"), [color](SlideObject &object) {
            object.fill.color = color;
            if (object.fill.kind == QLatin1String("linearGradient")
                && !object.fill.stops.isEmpty()) {
                object.fill.stops.first().color = color;
            }
        });
    });
    connect(m_gradientColor, &ColorButton::colorChanged, this, [this](const QColor &color) {
        if (m_updating)
            return;
        mutateObject(QStringLiteral("Change gradient"), [color](SlideObject &object) {
            if (object.fill.stops.isEmpty())
                object.fill.stops = {GradientStop{object.fill.color, 0.0},
                                     GradientStop{color, 1.0}};
            else
                object.fill.stops.last().color = color;
        });
    });
    connect(m_gradientAngle, &QDoubleSpinBox::editingFinished, this, [this] {
        if (m_updating)
            return;
        const double angle = m_gradientAngle->value();
        mutateObject(QStringLiteral("Change gradient angle"), [angle](SlideObject &object) {
            object.fill.gradientAngle = angle;
        });
    });
    connect(choose, &QPushButton::clicked, this, &InspectorPanel::chooseObjectMedia);
    connect(clear, &QPushButton::clicked, this, &InspectorPanel::clearObjectMedia);
    connect(m_mediaScale, QOverload<int>::of(&QComboBox::activated), this, [this](int index) {
        if (m_updating)
            return;
        const QString mode = m_mediaScale->itemData(index).toString();
        mutateObject(QStringLiteral("Change image scaling"), [mode](SlideObject &object) {
            object.mediaScaleMode = mode;
        });
    });
    connect(m_strokeEnabled, &QCheckBox::toggled, this, [this](bool enabled) {
        if (m_updating)
            return;
        mutateObject(QStringLiteral("Toggle outline"), [enabled](SlideObject &object) {
            object.stroke.enabled = enabled;
        });
    });
    connect(m_strokeColor, &ColorButton::colorChanged, this, [this](const QColor &color) {
        if (m_updating)
            return;
        mutateObject(QStringLiteral("Change outline color"), [color](SlideObject &object) {
            object.stroke.color = color;
        });
    });
    connect(m_strokeWidth, &QDoubleSpinBox::editingFinished, this, [this] {
        if (m_updating)
            return;
        const double width = m_strokeWidth->value();
        mutateObject(QStringLiteral("Change outline width"), [width](SlideObject &object) {
            object.stroke.width = width;
        });
    });
    connect(m_shadowEnabled, &QCheckBox::toggled, this, [this](bool enabled) {
        if (m_updating)
            return;
        mutateObject(QStringLiteral("Toggle shadow"), [enabled](SlideObject &object) {
            object.shadow.enabled = enabled;
        });
    });
    connect(m_shadowColor, &ColorButton::colorChanged, this, [this](const QColor &color) {
        if (m_updating)
            return;
        mutateObject(QStringLiteral("Change shadow color"), [color](SlideObject &object) {
            object.shadow.color = color;
        });
    });
    connect(m_shadowBlur, &QDoubleSpinBox::editingFinished, this, [this] {
        if (m_updating)
            return;
        const double blur = m_shadowBlur->value();
        mutateObject(QStringLiteral("Change shadow blur"), [blur](SlideObject &object) {
            object.shadow.blurRadius = blur;
        });
    });
    connect(m_shadowX, &QDoubleSpinBox::editingFinished, this, [this] {
        if (m_updating)
            return;
        const double x = m_shadowX->value();
        mutateObject(QStringLiteral("Change shadow offset"), [x](SlideObject &object) {
            object.shadow.offset.setX(x);
        });
    });
    connect(m_shadowY, &QDoubleSpinBox::editingFinished, this, [this] {
        if (m_updating)
            return;
        const double y = m_shadowY->value();
        mutateObject(QStringLiteral("Change shadow offset"), [y](SlideObject &object) {
            object.shadow.offset.setY(y);
        });
    });

    const auto applyFrame = [this] {
        if (m_updating)
            return;
        const double x = m_frameX->value();
        const double y = m_frameY->value();
        const double width = m_frameWidth->value();
        const double height = m_frameHeight->value();
        const double rotation = m_rotation->value();
        const double opacity = m_opacity->value() / 100.0;
        mutateObject(QStringLiteral("Change object frame"), [=](SlideObject &object) {
            object.hasFrame = true;
            object.frame = QRectF(x, y, std::max(1.0, width), std::max(1.0, height));
            object.rotationDegrees = rotation;
            object.opacity = std::clamp(opacity, 0.0, 1.0);
        });
    };
    for (QDoubleSpinBox *spin : {m_frameX, m_frameY, m_frameWidth, m_frameHeight, m_rotation,
                                 m_opacity})
        connect(spin, &QDoubleSpinBox::editingFinished, this, applyFrame);

    connect(m_duplicateObject, &QPushButton::clicked, m_editor, &Editor::duplicateSelectedObject);
    connect(m_forwardObject, &QPushButton::clicked, m_editor, &Editor::bringSelectedForward);
    connect(m_backwardObject, &QPushButton::clicked, m_editor, &Editor::sendSelectedBackward);
    connect(m_deleteObject, &QPushButton::clicked, m_editor, &Editor::deleteSelectedObject);
}

void InspectorPanel::buildDeckPage()
{
    auto *content = new QWidget;
    auto *outer = new QVBoxLayout(content);
    outer->setContentsMargins(2, 8, 2, 8);
    outer->setSpacing(10);

    auto *deckCard = new SectionCard(QStringLiteral("PRESENTATION"), content);
    auto *form = newForm();
    m_deckName = new QLineEdit;
    form->addRow(QStringLiteral("Name"), m_deckName);
    m_canvasSize = new QComboBox;
    m_canvasSize->addItem(QStringLiteral("1920 × 1080 · Full HD"), QStringLiteral("1920x1080"));
    m_canvasSize->addItem(QStringLiteral("1280 × 720 · HD"), QStringLiteral("1280x720"));
    m_canvasSize->addItem(QStringLiteral("3840 × 2160 · 4K"), QStringLiteral("3840x2160"));
    m_canvasSize->addItem(QStringLiteral("1080 × 1920 · Vertical"), QStringLiteral("1080x1920"));
    m_canvasSize->addItem(QStringLiteral("1366 × 768 · WXGA"), QStringLiteral("1366x768"));
    form->addRow(QStringLiteral("Canvas"), m_canvasSize);
    m_themeCombo = new QComboBox;
    form->addRow(QStringLiteral("Theme"), m_themeCombo);
    deckCard->body()->addLayout(form);
    outer->addWidget(deckCard);

    auto *themeCard = new SectionCard(QStringLiteral("THEME"), content);
    m_themeDetails = hintLabel(QString());
    themeCard->body()->addWidget(m_themeDetails);
    m_editTheme = new QPushButton(QStringLiteral("Edit theme…"));
    themeCard->body()->addWidget(m_editTheme);
    outer->addWidget(themeCard);

    auto *aboutCard = new SectionCard(QStringLiteral("DOCUMENT COMPATIBILITY"), content);
    aboutCard->body()->addWidget(hintLabel(
        QStringLiteral("This deck uses the shared .mxuslides document schema. "
                       "macOS-only fields that Windows does not edit are preserved when the "
                       "bundle is exported again.")));
    outer->addWidget(aboutCard);
    outer->addStretch();

    m_tabs->addTab(scrollPage(content), QStringLiteral("Deck"));

    connect(m_deckName, &QLineEdit::editingFinished, this, [this] {
        if (m_updating)
            return;
        const QString name = m_deckName->text().trimmed();
        if (name.isEmpty()) {
            refresh();
            return;
        }
        mutateDeck(QStringLiteral("Rename presentation"),
                   [name](Presentation &deck) { deck.name = name; });
    });
    connect(m_canvasSize, QOverload<int>::of(&QComboBox::activated), this, [this](int index) {
        if (m_updating)
            return;
        const QStringList parts = m_canvasSize->itemData(index).toString().split(QLatin1Char('x'));
        if (parts.size() != 2)
            return;
        const int width = parts.at(0).toInt();
        const int height = parts.at(1).toInt();
        mutateDeck(QStringLiteral("Change canvas size"), [width, height](Presentation &deck) {
            deck.canvasWidth = width;
            deck.canvasHeight = height;
        });
    });
    connect(m_themeCombo, QOverload<int>::of(&QComboBox::activated), this, [this](int index) {
        if (m_updating)
            return;
        const QString themeId = m_themeCombo->itemData(index).toString();
        mutateDeck(QStringLiteral("Change theme"),
                   [themeId](Presentation &deck) { deck.themeId = themeId; });
        m_editor->reloadThemes();
        m_editor->notifyEdited();
        updateDeckFields();
    });
    connect(m_editTheme, &QPushButton::clicked, this, &InspectorPanel::editTheme);
}

void InspectorPanel::refresh()
{
    const bool hasDeck = m_editor->hasPresentation();
    m_tabs->setEnabled(hasDeck);
    if (!hasDeck) {
        m_objectStack->setCurrentIndex(0);
        return;
    }

    updateSelectionTab();
    updateSlideFields();
    updateObjectFields();
    updateDeckFields();
}

void InspectorPanel::updateSlideFields()
{
    const Slide *slide = m_editor->currentSlide();
    if (!slide)
        return;
    UpdateGuard guard(m_updating);

    if (!m_slideName->hasFocus())
        m_slideName->setText(slide->name);

    QColor background = m_editor->theme().backgroundColor();
    if (slide->backgroundFill.visible()) {
        background = slide->backgroundFill.color.alpha() > 0
            ? slide->backgroundFill.color
            : (slide->backgroundFill.stops.isEmpty()
                   ? m_editor->theme().backgroundColor()
                   : slide->backgroundFill.stops.first().color);
    }
    m_slideBackground->setColor(background);
    m_slideBackgroundReset->setEnabled(slide->backgroundFill.visible());

    if (!m_notes->hasFocus())
        m_notes->setPlainText(slide->notes);

    const Transition &transition = slide->transition;
    m_transitionEnabled->setChecked(transition.enabled);
    m_transitionKind->setEnabled(transition.enabled);
    m_transitionDuration->setEnabled(transition.enabled);
    int transitionIndex = m_transitionKind->findData(transition.kind);
    if (transitionIndex < 0) {
        m_transitionKind->addItem(transition.kind, transition.kind);
        transitionIndex = m_transitionKind->count() - 1;
    }
    m_transitionKind->setCurrentIndex(transitionIndex);
    m_transitionDuration->setValue(transition.durationSeconds);
    m_deleteSlide->setEnabled(true);
}

void InspectorPanel::updateObjectFields()
{
    const SlideObject *object = m_editor->selectedObject();
    if (!object) {
        m_objectStack->setCurrentIndex(0);
        return;
    }

    m_objectStack->setCurrentIndex(1);
    UpdateGuard guard(m_updating);

    m_objectType->setText(kindLabel(*object));
    if (!m_objectName->hasFocus())
        m_objectName->setText(object->name);

    const bool isText = object->isText();
    const bool isShape = object->isShape();
    const bool isMedia = object->isMedia();
    m_textSection->setVisible(isText);
    m_shapeSection->setVisible(isShape);
    m_fillSection->setVisible(isShape);
    m_mediaSection->setVisible(isMedia);

    if (isText) {
        if (!m_objectText->hasFocus())
            m_objectText->setPlainText(object->text);

        int fontIndex = m_fontFamily->findData(object->textStyle.fontName);
        if (fontIndex < 0) {
            m_fontFamily->addItem(object->textStyle.fontName.isEmpty()
                                      ? QStringLiteral("Theme font")
                                      : object->textStyle.fontName,
                                  object->textStyle.fontName);
            fontIndex = m_fontFamily->count() - 1;
        }
        m_fontFamily->setCurrentIndex(fontIndex);
        m_fontSize->setValue(object->textStyle.fontSize);
        m_textColorSet->setChecked(object->textStyle.colorSet);
        m_textColor->setEnabled(object->textStyle.colorSet);
        m_textColor->setColor(object->textStyle.colorSet ? object->textStyle.color
                                                         : m_editor->theme().textColor());
        m_hAlignment->setCurrentValue(object->textStyle.horizontalAlignment);
        m_vAlignment->setCurrentValue(object->textStyle.verticalAlignment);
        m_autoShrink->setChecked(object->textStyle.autoShrink);
    }

    if (isShape) {
        int shapeIndex = m_shapeKind->findData(object->shapeKind);
        if (shapeIndex < 0) {
            m_shapeKind->addItem(object->shapeKind, object->shapeKind);
            shapeIndex = m_shapeKind->count() - 1;
        }
        m_shapeKind->setCurrentIndex(shapeIndex);
        m_cornerRadius->setValue(object->cornerRadius);
        m_cornerRadius->setEnabled(object->shapeKind == QLatin1String("roundedRectangle"));
    }

    int fillIndex = m_fillKind->findData(object->fill.kind);
    if (fillIndex < 0) {
        if (object->fill.kind == QLatin1String("media")) {
            m_fillKind->addItem(QStringLiteral("Media fill"), QStringLiteral("media"));
            fillIndex = m_fillKind->count() - 1;
        } else {
            fillIndex = 0;
        }
    }
    m_fillKind->setCurrentIndex(fillIndex);
    m_fillColor->setColor(object->fill.color.alpha() > 0 ? object->fill.color
                                                          : m_editor->theme().textColor());
    const QColor gradientEnd = object->fill.stops.size() > 1
        ? object->fill.stops.last().color
        : object->fill.color.darker(170);
    m_gradientColor->setColor(gradientEnd);
    m_gradientAngle->setValue(object->fill.gradientAngle);
    syncFillControls();

    if (isMedia) {
        const QString path = m_editor->mediaPath(object->mediaId);
        m_mediaName->setText(path.isEmpty()
                                 ? QStringLiteral("No image selected")
                                 : QStringLiteral("%1\n%2")
                                       .arg(QFileInfo(path).fileName(), QFileInfo(path).absolutePath()));
        int scaleIndex = m_mediaScale->findData(object->mediaScaleMode);
        m_mediaScale->setCurrentIndex(scaleIndex >= 0 ? scaleIndex : 0);
    }

    m_strokeEnabled->setChecked(object->stroke.enabled);
    m_strokeColor->setColor(object->stroke.color);
    m_strokeColor->setEnabled(object->stroke.enabled);
    m_strokeWidth->setValue(object->stroke.width);
    m_strokeWidth->setEnabled(object->stroke.enabled);

    m_shadowEnabled->setChecked(object->shadow.enabled);
    m_shadowColor->setColor(object->shadow.color);
    m_shadowColor->setEnabled(object->shadow.enabled);
    m_shadowBlur->setEnabled(object->shadow.enabled);
    m_shadowBlur->setValue(object->shadow.blurRadius);
    m_shadowX->setEnabled(object->shadow.enabled);
    m_shadowX->setValue(object->shadow.offset.x());
    m_shadowY->setEnabled(object->shadow.enabled);
    m_shadowY->setValue(object->shadow.offset.y());

    const QRectF frame = displayFrame(*object, m_editor->presentation());
    m_frameX->setValue(frame.x());
    m_frameY->setValue(frame.y());
    m_frameWidth->setValue(frame.width());
    m_frameHeight->setValue(frame.height());
    m_rotation->setValue(object->rotationDegrees);
    m_opacity->setValue(object->opacity * 100.0);
}

void InspectorPanel::updateDeckFields()
{
    if (!m_editor->hasPresentation())
        return;
    UpdateGuard guard(m_updating);
    const Presentation &deck = m_editor->presentation();

    if (!m_deckName->hasFocus())
        m_deckName->setText(deck.name);

    const QString canvas = QStringLiteral("%1x%2").arg(deck.canvasWidth).arg(deck.canvasHeight);
    int canvasIndex = m_canvasSize->findData(canvas);
    if (canvasIndex < 0) {
        m_canvasSize->addItem(QStringLiteral("%1 × %2").arg(deck.canvasWidth).arg(deck.canvasHeight),
                              canvas);
        canvasIndex = m_canvasSize->count() - 1;
    }
    m_canvasSize->setCurrentIndex(canvasIndex);

    {
        QSignalBlocker blocker(m_themeCombo);
        m_themeCombo->clear();
        for (const Theme &theme : m_editor->themes())
            m_themeCombo->addItem(theme.name, theme.id);
        const int themeIndex = m_themeCombo->findData(deck.themeId);
        m_themeCombo->setCurrentIndex(themeIndex >= 0 ? themeIndex : 0);
    }

    const Theme theme = m_editor->theme();
    m_themeDetails->setText(
        QStringLiteral("%1\n%2 · %3 px\nText %4 · Background %5")
            .arg(theme.name, theme.fontFamily)
            .arg(theme.fontSize)
            .arg(theme.textColorHex, theme.backgroundColorHex));
}

void InspectorPanel::updateSelectionTab()
{
    const QString selected = m_editor->selectedObjectId();
    if (selected == m_lastSelectedId)
        return;
    if (!selected.isEmpty())
        m_tabs->setCurrentIndex(1);
    else if (m_tabs->currentIndex() == 1)
        m_tabs->setCurrentIndex(0);
    m_lastSelectedId = selected;
}

void InspectorPanel::updateFontFamilies()
{
    if (!m_fontFamily)
        return;
    const QString previous = m_fontFamily->currentData().toString();
    m_fontFamily->clear();
    m_fontFamily->addItem(QStringLiteral("Theme font"), QString());
    for (const QString &family : QFontDatabase::families())
        m_fontFamily->addItem(family, family);
    if (!previous.isEmpty()) {
        const int index = m_fontFamily->findData(previous);
        if (index >= 0)
            m_fontFamily->setCurrentIndex(index);
    }
    m_fontsReady = true;
}

void InspectorPanel::syncFillControls()
{
    if (!m_fillKind || !m_fillColor || !m_gradientRow)
        return;
    const QString kind = m_fillKind->currentData().toString();
    const bool gradient = kind == QLatin1String("linearGradient");
    const bool filled = kind != QLatin1String("none");
    m_fillColor->setEnabled(filled);
    m_gradientColor->setEnabled(gradient);
    m_gradientAngle->setEnabled(gradient);
    if (m_fillForm)
        m_fillForm->setRowVisible(m_gradientRow, gradient);
}

void InspectorPanel::mutateSlide(const QString &label,
                                 const std::function<void(Slide &)> &edit)
{
    if (!m_editor->hasPresentation())
        return;
    const int index = m_editor->currentIndex();
    if (index < 0)
        return;
    m_editor->commit(label, [index, edit](Presentation &deck) {
        if (index < 0 || index >= deck.slides.size())
            return;
        edit(deck.slides[index]);
    });
}

void InspectorPanel::mutateObject(const QString &label,
                                  const std::function<void(SlideObject &)> &edit)
{
    const QString id = m_editor->selectedObjectId();
    const int index = m_editor->currentIndex();
    if (id.isEmpty() || index < 0)
        return;
    m_editor->commit(label, [index, id, edit](Presentation &deck) {
        if (index < 0 || index >= deck.slides.size())
            return;
        for (SlideObject &object : deck.slides[index].objects) {
            if (object.id == id) {
                edit(object);
                break;
            }
        }
    });
}

void InspectorPanel::mutateDeck(const QString &label,
                                const std::function<void(Presentation &)> &edit)
{
    if (m_editor->hasPresentation())
        m_editor->commit(label, edit);
}

void InspectorPanel::scheduleTextEdit()
{
    const SlideObject *object = m_editor->selectedObject();
    if (!object || !object->isText())
        return;
    m_pendingObjectId = object->id;
    m_pendingObjectSlide = m_editor->currentIndex();
    m_pendingText = m_objectText->toPlainText();
    m_textTimer->start();
}

void InspectorPanel::applyPendingText()
{
    if (m_pendingObjectId.isEmpty())
        return;
    const QString id = m_pendingObjectId;
    const int slideIndex = m_pendingObjectSlide;
    const QString text = m_pendingText;
    m_pendingObjectId.clear();
    m_pendingText.clear();
    m_pendingObjectSlide = -1;
    m_editor->commit(QStringLiteral("Edit text"), [id, slideIndex, text](Presentation &deck) {
        if (slideIndex < 0 || slideIndex >= deck.slides.size())
            return;
        for (SlideObject &object : deck.slides[slideIndex].objects) {
            if (object.id == id) {
                object.text = text;
                break;
            }
        }
    });
}

void InspectorPanel::scheduleNotesEdit()
{
    const Slide *slide = m_editor->currentSlide();
    if (!slide)
        return;
    m_pendingNotesSlideId = slide->id;
    m_pendingNotes = m_notes->toPlainText();
    m_notesTimer->start();
}

void InspectorPanel::applyPendingNotes()
{
    if (m_pendingNotesSlideId.isEmpty())
        return;
    const QString slideId = m_pendingNotesSlideId;
    const QString notes = m_pendingNotes;
    m_pendingNotesSlideId.clear();
    m_pendingNotes.clear();
    m_editor->commit(QStringLiteral("Edit speaker notes"), [slideId, notes](Presentation &deck) {
        for (Slide &slide : deck.slides) {
            if (slide.id == slideId) {
                slide.notes = notes;
                break;
            }
        }
    });
}

void InspectorPanel::flushPendingEdits()
{
    if (m_textTimer && m_textTimer->isActive()) {
        m_textTimer->stop();
        applyPendingText();
    }
    if (m_notesTimer && m_notesTimer->isActive()) {
        m_notesTimer->stop();
        applyPendingNotes();
    }
}

void InspectorPanel::focusTextEditor()
{
    if (!m_editor->selectedObject() || !m_editor->selectedObject()->isText())
        return;
    m_tabs->setCurrentIndex(1);
    m_objectText->setFocus(Qt::ShortcutFocusReason);
    m_objectText->selectAll();
}

void InspectorPanel::resetSlideBackground()
{
    mutateSlide(QStringLiteral("Use theme background"),
                [](Slide &slide) { slide.backgroundFill = Fill(); });
}

void InspectorPanel::chooseObjectMedia()
{
    if (!m_editor->selectedObject() || !m_editor->selectedObject()->isMedia())
        return;
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Choose image"), QString(),
        QStringLiteral("Images (*.png *.jpg *.jpeg *.bmp *.webp *.gif *.tif *.tiff);;All files (*)"));
    if (path.isEmpty())
        return;
    QString error;
    QString warning;
    const QString mediaId = m_editor->importMediaFile(path, &error, &warning);
    if (mediaId.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Choose image"), error);
        return;
    }
    if (!warning.isEmpty())
        QMessageBox::information(this, QStringLiteral("Choose image"), warning);
    mutateObject(QStringLiteral("Replace image"),
                 [mediaId](SlideObject &object) { object.mediaId = mediaId; });
}

void InspectorPanel::clearObjectMedia()
{
    mutateObject(QStringLiteral("Remove image"),
                 [](SlideObject &object) { object.mediaId.clear(); });
}

void InspectorPanel::editTheme()
{
    if (!m_editor->hasPresentation())
        return;
    ThemeDialog dialog(m_editor->theme(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    QString error;
    if (!m_editor->library()->saveTheme(dialog.result(), &error)) {
        QMessageBox::warning(this, QStringLiteral("Edit theme"), error);
        return;
    }
    m_editor->reloadThemes();
    m_editor->notifyEdited();
    updateDeckFields();
}

} // namespace mxu::ui
