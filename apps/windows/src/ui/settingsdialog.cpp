#include "ui/settingsdialog.h"

#include "ui/widgets.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScreen>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>

namespace mxu::ui {
namespace {

void populateScreens(QComboBox *combo, const QString &automaticLabel, const QString &selected)
{
    combo->clear();
    combo->addItem(automaticLabel, QString());
    const QScreen *primary = QGuiApplication::primaryScreen();
    int fallbackIndex = 0;
    for (QScreen *screen : QGuiApplication::screens()) {
        QString label = QStringLiteral("%1 · %2×%3")
                            .arg(screen->name())
                            .arg(screen->geometry().width())
                            .arg(screen->geometry().height());
        if (!screen->name().isEmpty())
            label = screen->name() + QStringLiteral("  ·  %1×%2")
                                        .arg(screen->geometry().width())
                                        .arg(screen->geometry().height());
        if (screen == primary)
            label += QStringLiteral("  (Primary)");
        combo->addItem(label, screen->name());
        if (screen->name() == selected)
            fallbackIndex = combo->count() - 1;
    }
    combo->setCurrentIndex(selected.isEmpty() ? 0 : fallbackIndex);
}

} // namespace

SettingsDialog::SettingsDialog(const QString &libraryRoot, const QString &outputScreen,
                               bool stageEnabled, const QString &stageScreen, int timerSeconds,
                               QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("MxU Slides settings"));
    setMinimumWidth(540);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 18, 20, 16);
    layout->setSpacing(12);

    auto *title = makeLabel(QStringLiteral("Settings"), QStringLiteral("Title"));
    layout->addWidget(title);
    auto *subtitle = makeLabel(
        QStringLiteral("Local-first presentation tools for Windows. Changes are saved to this PC."),
        QStringLiteral("Muted"));
    subtitle->setWordWrap(true);
    layout->addWidget(subtitle);

    auto *libraryCard = new SectionCard(QStringLiteral("LIBRARY"), this);
    auto *libraryRow = new QHBoxLayout;
    libraryRow->setSpacing(6);
    m_libraryRoot = new QLineEdit(libraryRoot);
    auto *browse = new QPushButton(QStringLiteral("Browse…"));
    libraryRow->addWidget(m_libraryRoot, 1);
    libraryRow->addWidget(browse);
    libraryCard->body()->addLayout(libraryRow);
    libraryCard->body()->addWidget(makeLabel(
        QStringLiteral("Presentations, themes, media, and .mxuslides imports live in this folder."),
        QStringLiteral("Faint")));
    layout->addWidget(libraryCard);

    auto *outputCard = new SectionCard(QStringLiteral("DISPLAYS"), this);
    auto *form = new QFormLayout;
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(9);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    m_outputScreen = new QComboBox;
    populateScreens(m_outputScreen,
                    QStringLiteral("Windowed preview (no dedicated output display)"),
                    outputScreen);
    form->addRow(QStringLiteral("Program output"), m_outputScreen);

    m_stageEnabled = new QCheckBox(QStringLiteral("Show the stage display"));
    form->addRow(QStringLiteral("Stage"), m_stageEnabled);

    m_stageScreen = new QComboBox;
    populateScreens(m_stageScreen,
                    QStringLiteral("Automatic external display"), stageScreen);
    form->addRow(QStringLiteral("Stage display"), m_stageScreen);
    outputCard->body()->addLayout(form);
    outputCard->body()->addWidget(makeLabel(
        QStringLiteral("Use Windowed preview to rehearse with the program window on this screen."),
        QStringLiteral("Faint")));
    layout->addWidget(outputCard);

    auto *timerCard = new SectionCard(QStringLiteral("DEFAULT TIMER"), this);
    auto *timerForm = new QFormLayout;
    timerForm->setHorizontalSpacing(12);
    m_timerMinutes = new QSpinBox;
    m_timerMinutes->setRange(1, 180);
    m_timerMinutes->setSuffix(QStringLiteral(" min"));
    m_timerMinutes->setValue(std::max(1, timerSeconds / 60));
    timerForm->addRow(QStringLiteral("Duration"), m_timerMinutes);
    timerCard->body()->addLayout(timerForm);
    layout->addWidget(timerCard);
    layout->addStretch();

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);

    connect(browse, &QPushButton::clicked, this, &SettingsDialog::browseLibrary);
    connect(m_stageEnabled, &QCheckBox::toggled, m_stageScreen, &QComboBox::setEnabled);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    m_stageEnabled->setChecked(stageEnabled);
    m_stageScreen->setEnabled(stageEnabled);
}

QString SettingsDialog::libraryRoot() const
{
    return m_libraryRoot->text().trimmed();
}

QString SettingsDialog::outputScreen() const
{
    return m_outputScreen->currentData().toString();
}

bool SettingsDialog::stageEnabled() const
{
    return m_stageEnabled->isChecked();
}

QString SettingsDialog::stageScreen() const
{
    return m_stageScreen->currentData().toString();
}

int SettingsDialog::timerSeconds() const
{
    return m_timerMinutes->value() * 60;
}

void SettingsDialog::browseLibrary()
{
    const QString path = QFileDialog::getExistingDirectory(
        this, QStringLiteral("Choose the MxU Slides library folder"), m_libraryRoot->text());
    if (!path.isEmpty())
        m_libraryRoot->setText(path);
}

} // namespace mxu::ui
