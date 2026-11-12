#include "ui/widgets.h"

#include "ui/theme.h"

#include <QButtonGroup>
#include <QColorDialog>
#include <QSizePolicy>
#include <algorithm>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPaintEvent>
#include <QVBoxLayout>

namespace mxu::ui {

ColorButton::ColorButton(QWidget *parent)
    : QToolButton(parent)
{
    setCursor(Qt::PointingHandCursor);
    setFixedHeight(28);
    setMinimumWidth(92);
    setToolTip(m_color.name(QColor::HexArgb).toUpper());
    connect(this, &QToolButton::clicked, this, [this] {
        const QColor chosen = QColorDialog::getColor(m_color, this, QStringLiteral("Choose color"),
                                                     QColorDialog::ShowAlphaChannel);
        if (!chosen.isValid() || chosen == m_color)
            return;
        setColor(chosen);
        emit colorChanged(chosen);
    });
}

void ColorButton::setColor(const QColor &color)
{
    if (!color.isValid() || color == m_color)
        return;
    m_color = color;
    setToolTip(m_color.name(QColor::HexArgb).toUpper());
    update();
}

void ColorButton::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const Palette &p = mxu::ui::palette();
    const QRectF outer = rect().adjusted(0.5, 0.5, -0.5, -0.5);
    painter.setPen(QPen(underMouse() ? p.borderStrong : p.border, 1));
    painter.setBrush(p.surface);
    painter.drawRoundedRect(outer, 7, 7);

    const QRectF swatch(6, 6, 16, 16);
    painter.setPen(QPen(QColor(255, 255, 255, 30), 1));
    painter.setBrush(m_color.alpha() == 0 ? QColor(40, 44, 50) : m_color);
    painter.drawRoundedRect(swatch, 5, 5);

    painter.setPen(m_color.lightness() > 150 ? QColor(0, 0, 0, 60) : QColor(255, 255, 255, 30));
    painter.drawLine(QPointF(7, 20), QPointF(21, 7));

    painter.setPen(p.text);
    QFont labelFont = font();
    labelFont.setPointSize(8);
    painter.setFont(labelFont);
    painter.drawText(QRectF(28, 0, width() - 34, height()), Qt::AlignVCenter | Qt::AlignLeft,
                     m_color.name(QColor::HexRgb).toUpper());
}

SegmentedControl::SegmentedControl(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
}

void SegmentedControl::addItem(const QString &label, const QString &value)
{
    auto *button = new QToolButton(this);
    button->setText(label);
    button->setObjectName(QStringLiteral("Segment"));
    button->setCheckable(true);
    button->setCursor(Qt::PointingHandCursor);
    button->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    layout()->addWidget(button);
    m_items.append(Item{value, button});
    connect(button, &QToolButton::clicked, this, [this, value] {
        if (m_setting)
            return;
        setCurrentValue(value);
        emit changed(value);
    });
    if (m_current.isEmpty())
        setCurrentValue(value);
}

void SegmentedControl::setCurrentValue(const QString &value)
{
    if (!m_items.isEmpty()
        && std::none_of(m_items.begin(), m_items.end(),
                        [&](const Item &item) { return item.value == value; })) {
        return;
    }
    m_setting = true;
    m_current = value;
    for (const Item &item : m_items)
        item.button->setChecked(item.value == value);
    m_setting = false;
}

QString SegmentedControl::currentValue() const
{
    return m_current;
}

void SegmentedControl::setEnabledFor(const QString &value, bool enabled)
{
    for (const Item &item : m_items) {
        if (item.value == value)
            item.button->setEnabled(enabled);
    }
}

TimerChip::TimerChip(QWidget *parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("Chip"));
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(6, 1, 4, 1);
    layout->setSpacing(1);

    m_time = new QToolButton(this);
    m_time->setObjectName(QStringLiteral("TimerTime"));
    m_time->setText(QStringLiteral("05:00"));
    m_time->setCursor(Qt::PointingHandCursor);
    m_time->setMinimumWidth(48);
    m_time->setToolTip(QStringLiteral("Set the timer duration"));
    layout->addWidget(m_time);

    auto *play = makeIconButton(Glyph::Play, QStringLiteral("Start or pause the timer"), this);
    play->setCheckable(true);
    layout->addWidget(play);

    auto *reset = makeIconButton(Glyph::Reset, QStringLiteral("Reset the timer"), this);
    layout->addWidget(reset);

    m_toggle = play;
    connect(play, &QToolButton::clicked, this, &TimerChip::toggleRequested);
    connect(reset, &QToolButton::clicked, this, &TimerChip::resetRequested);
    connect(m_time, &QToolButton::clicked, this, &TimerChip::configureRequested);
}

void TimerChip::setRemainingSeconds(int seconds)
{
    m_time->setText(ui::formatTimer(seconds));
}

void TimerChip::setRunning(bool running)
{
    m_running = running;
    m_toggle->setChecked(running);
    m_toggle->setIcon(ui::icon(running ? Glyph::Pause : Glyph::Play,
                               mxu::ui::palette().text, 15));
    m_toggle->setToolTip(running ? QStringLiteral("Pause the timer")
                                 : QStringLiteral("Start the timer"));
}

SectionCard::SectionCard(const QString &title, QWidget *parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("Card"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 10, 12, 12);
    layout->setSpacing(8);
    auto *label = makeLabel(title, QStringLiteral("SectionTitle"));
    layout->addWidget(label);
    m_body = new QVBoxLayout();
    m_body->setContentsMargins(0, 0, 0, 0);
    m_body->setSpacing(7);
    layout->addLayout(m_body);
}

QToolButton *makeIconButton(Glyph glyph, const QString &tooltip, QWidget *parent)
{
    auto *button = new QToolButton(parent);
    button->setObjectName(QStringLiteral("IconButton"));
    button->setIcon(ui::icon(glyph, mxu::ui::palette().textMuted, 17));
    button->setIconSize(QSize(17, 17));
    button->setToolTip(tooltip);
    button->setCursor(Qt::PointingHandCursor);
    button->setAutoRaise(false);
    return button;
}

QLabel *makeLabel(const QString &text, const QString &objectName, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    if (!objectName.isEmpty())
        label->setObjectName(objectName);
    return label;
}

QFrame *makeSeparator(QWidget *parent)
{
    auto *line = new QFrame(parent);
    line->setFrameShape(QFrame::HLine);
    line->setFixedHeight(1);
    line->setStyleSheet(QStringLiteral("background: %1; border: none;").arg(mxu::ui::palette().border.name()));
    return line;
}

} // namespace mxu::ui
