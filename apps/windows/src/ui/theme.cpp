#include "ui/theme.h"

#include <QApplication>
#include <QFontDatabase>
#include <QPalette>
#include <QStyleFactory>

namespace mxu::ui {
namespace {

Palette makePalette()
{
    Palette p;
    p.window = QColor(18, 19, 22);
    p.panel = QColor(24, 26, 30);
    p.surface = QColor(31, 34, 39);
    p.surfaceRaised = QColor(39, 43, 49);
    p.border = QColor(48, 53, 61);
    p.borderStrong = QColor(67, 73, 83);
    p.text = QColor(243, 244, 246);
    p.textMuted = QColor(163, 170, 181);
    p.textFaint = QColor(112, 120, 133);
    p.accent = QColor(91, 141, 239);
    p.accentHover = QColor(112, 159, 247);
    p.accentSoft = QColor(91, 141, 239, 42);
    p.live = QColor(255, 91, 86);
    p.warning = QColor(240, 179, 91);
    p.success = QColor(73, 196, 138);
    p.shadow = QColor(0, 0, 0, 110);
    return p;
}

QString makeStyleSheet(const Palette &p)
{
    QString style = QStringLiteral(R"(
QWidget {
    color: %TEXT%;
    font-size: 10pt;
}
QMainWindow, QDialog {
    background: %WINDOW%;
}
QMenuBar {
    background: %PANEL%;
    border-bottom: 1px solid %BORDER%;
    padding: 2px 6px;
}
QMenuBar::item {
    background: transparent;
    padding: 4px 9px;
    border-radius: 5px;
}
QMenuBar::item:selected {
    background: %SURFACE%;
}
QMenu {
    background: %SURFACE%;
    border: 1px solid %BORDER%;
    border-radius: 8px;
    padding: 5px;
}
QMenu::item {
    padding: 6px 30px 6px 12px;
    border-radius: 5px;
}
QMenu::item:selected {
    background: %ACCENT_SOFT%;
}
QMenu::separator {
    height: 1px;
    background: %BORDER%;
    margin: 5px 8px;
}
QStatusBar {
    background: %PANEL%;
    border-top: 1px solid %BORDER%;
    color: %MUTED%;
    font-size: 9pt;
}
QStatusBar::item { border: none; }
QToolTip {
    background: %SURFACE_RAISED%;
    color: %TEXT%;
    border: 1px solid %BORDER%;
    border-radius: 6px;
    padding: 5px 8px;
}
QFrame#Header {
    background: %PANEL%;
    border-bottom: 1px solid %BORDER%;
}
QWidget#SidePanel {
    background: %PANEL%;
    border-right: 1px solid %BORDER%;
}
QWidget#InspectorPanel {
    background: %PANEL%;
    border-left: 1px solid %BORDER%;
}
QWidget#EditorPage, QWidget#PresenterPage, QWidget#CanvasHost {
    background: %WINDOW%;
}
QFrame#Card {
    background: %SURFACE%;
    border: 1px solid %BORDER%;
    border-radius: 10px;
}
QLabel#Muted { color: %MUTED%; }
QLabel#Faint { color: %FAINT%; }
QLabel#AppTitle { font-size: 12pt; font-weight: 600; }
QLabel#AppSubtitle { color: %FAINT%; font-size: 8pt; }
QLabel#SectionTitle {
    color: %MUTED%;
    font-size: 8pt;
    font-weight: 600;
}
QLabel#Title { font-size: 16pt; font-weight: 600; }
QLabel#SlidePosition { color: %MUTED%; font-size: 9pt; }
QLabel#Chip {
    background: %SURFACE%;
    border: 1px solid %BORDER%;
    border-radius: 8px;
    padding: 4px 8px;
    color: %MUTED%;
}
QLineEdit, QTextEdit, QPlainTextEdit, QComboBox, QSpinBox, QDoubleSpinBox {
    background: %SURFACE%;
    border: 1px solid %BORDER%;
    border-radius: 7px;
    padding: 5px 8px;
    selection-background-color: %ACCENT%;
    selection-color: #FFFFFF;
}
QLineEdit:hover, QTextEdit:hover, QPlainTextEdit:hover, QComboBox:hover,
QSpinBox:hover, QDoubleSpinBox:hover {
    border-color: %BORDER_STRONG%;
}
QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus, QComboBox:focus,
QSpinBox:focus, QDoubleSpinBox:focus {
    border-color: %ACCENT%;
}
QLineEdit:disabled, QComboBox:disabled, QSpinBox:disabled, QDoubleSpinBox:disabled {
    color: %FAINT%;
}
QComboBox::drop-down {
    border: none;
    width: 18px;
}
QComboBox QAbstractItemView {
    background: %SURFACE%;
    border: 1px solid %BORDER%;
    border-radius: 6px;
    padding: 4px;
    selection-background-color: %ACCENT_SOFT%;
    selection-color: %TEXT%;
}
QPushButton {
    background: %SURFACE%;
    border: 1px solid %BORDER%;
    border-radius: 7px;
    padding: 6px 12px;
    min-height: 18px;
}
QPushButton:hover { background: %SURFACE_RAISED%; border-color: %BORDER_STRONG%; }
QPushButton:pressed { background: %PANEL%; }
QPushButton:disabled { color: %FAINT%; }
QPushButton#Accent {
    background: %ACCENT%;
    border: 1px solid %ACCENT%;
    color: #FFFFFF;
    font-weight: 600;
}
QPushButton#Accent:hover { background: %ACCENT_HOVER%; border-color: %ACCENT_HOVER%; }
QPushButton#Live {
    background: %LIVE%;
    border: 1px solid %LIVE%;
    color: #FFFFFF;
    font-weight: 600;
}
QPushButton#Live:hover { background: #FF726D; border-color: #FF726D; }
QPushButton#Danger {
    background: transparent;
    border: 1px solid %BORDER%;
    color: %LIVE%;
}
QPushButton#Danger:hover { background: rgba(255, 91, 86, 26); border-color: %LIVE%; }
QPushButton#Ghost {
    background: transparent;
    border: 1px solid transparent;
    color: %MUTED%;
}
QPushButton#Ghost:hover { background: %SURFACE%; color: %TEXT%; }
QToolButton {
    background: transparent;
    border: 1px solid transparent;
    border-radius: 6px;
    padding: 4px;
}
QToolButton:hover { background: %SURFACE_RAISED%; }
QToolButton:pressed { background: %PANEL%; }
QToolButton:checked { background: %ACCENT_SOFT%; border-color: %ACCENT%; }
QToolButton#IconButton { padding: 5px; }
QToolButton#IconButton:hover { background: %SURFACE%; }
QToolButton#Segment {
    border-radius: 6px;
    padding: 5px 12px;
    color: %MUTED%;
    font-weight: 500;
}
QToolButton#Segment:hover { color: %TEXT%; background: %SURFACE%; }
QToolButton#Segment:checked {
    background: %SURFACE_RAISED%;
    color: %TEXT%;
    border: 1px solid %BORDER%;
}
QToolButton#TimerTime {
    background: transparent;
    border: none;
    color: %TEXT%;
    font-size: 11pt;
    font-weight: 600;
    padding: 2px 4px;
}
QToolButton#TimerTime:hover { color: %ACCENT_HOVER%; }
QFrame#Chip {
    background: %SURFACE%;
    border: 1px solid %BORDER%;
    border-radius: 8px;
}
QListWidget {
    background: transparent;
    border: none;
    outline: none;
}
QListWidget#LibraryList::item {
    background: transparent;
    border: none;
}
QListWidget#LibraryList::item:selected { background: transparent; }
QListWidget#SlideList {
    background: transparent;
    border: none;
    outline: none;
}
QScrollArea { background: transparent; border: none; }
QScrollArea > QWidget > QWidget { background: transparent; }
QScrollBar:vertical {
    background: transparent;
    width: 10px;
    margin: 2px;
}
QScrollBar::handle:vertical {
    background: %BORDER_STRONG%;
    min-height: 28px;
    border-radius: 4px;
}
QScrollBar::handle:vertical:hover { background: %MUTED%; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
QScrollBar:horizontal {
    background: transparent;
    height: 10px;
    margin: 2px;
}
QScrollBar::handle:horizontal {
    background: %BORDER_STRONG%;
    min-width: 28px;
    border-radius: 4px;
}
QScrollBar::handle:horizontal:hover { background: %MUTED%; }
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0px; }
QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: transparent; }
QSplitter::handle { background: transparent; }
QSplitter::handle:hover { background: %ACCENT_SOFT%; }
QTabWidget::pane { border: none; }
QTabBar::tab {
    background: transparent;
    color: %MUTED%;
    padding: 6px 12px;
    margin-right: 2px;
    border-radius: 6px;
}
QTabBar::tab:hover { color: %TEXT%; background: %SURFACE%; }
QTabBar::tab:selected {
    background: %SURFACE%;
    color: %TEXT%;
    font-weight: 600;
}
QCheckBox { spacing: 8px; }
QCheckBox::indicator {
    width: 16px;
    height: 16px;
    border-radius: 4px;
    border: 1px solid %BORDER_STRONG%;
    background: %SURFACE%;
}
QCheckBox::indicator:checked {
    background: %ACCENT%;
    border-color: %ACCENT%;
}
QSlider::groove:horizontal {
    height: 4px;
    background: %BORDER%;
    border-radius: 2px;
}
QSlider::sub-page:horizontal { background: %ACCENT%; border-radius: 2px; }
QSlider::handle:horizontal {
    background: %TEXT%;
    width: 14px;
    margin: -5px 0;
    border-radius: 7px;
}
QFormLayout QLabel { color: %MUTED%; }
)");
    style.replace(QStringLiteral("%WINDOW%"), p.window.name());
    style.replace(QStringLiteral("%PANEL%"), p.panel.name());
    style.replace(QStringLiteral("%SURFACE_RAISED%"), p.surfaceRaised.name());
    style.replace(QStringLiteral("%SURFACE%"), p.surface.name());
    style.replace(QStringLiteral("%BORDER_STRONG%"), p.borderStrong.name());
    style.replace(QStringLiteral("%BORDER%"), p.border.name());
    style.replace(QStringLiteral("%ACCENT_SOFT%"), QStringLiteral("rgba(%1, %2, %3, %4)")
                                                       .arg(p.accentSoft.red())
                                                       .arg(p.accentSoft.green())
                                                       .arg(p.accentSoft.blue())
                                                       .arg(p.accentSoft.alpha()));
    style.replace(QStringLiteral("%ACCENT_HOVER%"), p.accentHover.name());
    style.replace(QStringLiteral("%ACCENT%"), p.accent.name());
    style.replace(QStringLiteral("%LIVE%"), p.live.name());
    style.replace(QStringLiteral("%MUTED%"), p.textMuted.name());
    style.replace(QStringLiteral("%FAINT%"), p.textFaint.name());
    style.replace(QStringLiteral("%TEXT%"), p.text.name());
    return style;
}

} // namespace

const Palette &palette()
{
    static const Palette value = makePalette();
    return value;
}

QFont uiFont(int pointSize, QFont::Weight weight)
{
    QFont font = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
    font.setPointSize(pointSize);
    font.setWeight(weight);
    return font;
}

QString formatTimer(int seconds)
{
    seconds = qMax(0, seconds);
    const int hours = seconds / 3600;
    const int minutes = (seconds % 3600) / 60;
    const int remainder = seconds % 60;
    if (hours > 0) {
        return QStringLiteral("%1:%2:%3")
            .arg(hours, 2, 10, QLatin1Char('0'))
            .arg(minutes, 2, 10, QLatin1Char('0'))
            .arg(remainder, 2, 10, QLatin1Char('0'));
    }
    return QStringLiteral("%1:%2")
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(remainder, 2, 10, QLatin1Char('0'));
}

void applyTheme(QApplication &application)
{
    const Palette &p = palette();
    application.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    QPalette qpalette;
    qpalette.setColor(QPalette::Window, p.window);
    qpalette.setColor(QPalette::WindowText, p.text);
    qpalette.setColor(QPalette::Base, p.surface);
    qpalette.setColor(QPalette::AlternateBase, p.panel);
    qpalette.setColor(QPalette::ToolTipBase, p.surfaceRaised);
    qpalette.setColor(QPalette::ToolTipText, p.text);
    qpalette.setColor(QPalette::Text, p.text);
    qpalette.setColor(QPalette::Button, p.surface);
    qpalette.setColor(QPalette::ButtonText, p.text);
    qpalette.setColor(QPalette::BrightText, Qt::white);
    qpalette.setColor(QPalette::Highlight, p.accent);
    qpalette.setColor(QPalette::HighlightedText, Qt::white);
    qpalette.setColor(QPalette::Link, p.accent);
    qpalette.setColor(QPalette::PlaceholderText, p.textFaint);
    qpalette.setColor(QPalette::Disabled, QPalette::Text, p.textFaint);
    qpalette.setColor(QPalette::Disabled, QPalette::ButtonText, p.textFaint);
    application.setPalette(qpalette);
    application.setFont(uiFont(10));
    application.setStyleSheet(makeStyleSheet(p));
}

} // namespace mxu::ui
