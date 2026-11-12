#include "ui/stagewindow.h"

#include "core/editor.h"
#include "ui/icons.h"
#include "ui/theme.h"

#include <QCloseEvent>
#include <QFontMetricsF>
#include <QLinearGradient>
#include <QPainter>
#include <QScreen>
#include <QTime>
#include <QTimer>
#include <QWindow>

#include <algorithm>

namespace mxu::ui {
namespace {

void drawFittedText(QPainter &painter, const QRectF &bounds, const QString &text,
                    const QColor &color, int maxPixelSize, int minPixelSize,
                    QFont::Weight weight = QFont::DemiBold)
{
    if (text.trimmed().isEmpty() || bounds.width() <= 1.0 || bounds.height() <= 1.0)
        return;
    const int flags = Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap;
    for (int pixelSize = maxPixelSize; pixelSize >= minPixelSize; pixelSize -= 2) {
        QFont font = uiFont(10, weight);
        font.setPixelSize(pixelSize);
        const QFontMetricsF metrics(font);
        const QRectF needed = metrics.boundingRect(bounds, flags, text);
        if (needed.height() <= bounds.height() + 1.0) {
            painter.setFont(font);
            painter.setPen(color);
            painter.drawText(bounds, flags, text);
            return;
        }
    }
    QFont font = uiFont(10, weight);
    font.setPixelSize(minPixelSize);
    painter.save();
    painter.setClipRect(bounds);
    painter.setFont(font);
    painter.setPen(color);
    painter.drawText(bounds, flags, text);
    painter.restore();
}

QString slideText(const Slide &slide)
{
    const QString text = slidePlainText(slide);
    return text.trimmed().isEmpty() ? slide.name : text;
}

} // namespace

StageWindow::StageWindow(Editor *editor, SlideRenderer *renderer, QWidget *parent)
    : QWidget(parent)
    , m_editor(editor)
    , m_renderer(renderer)
{
    setObjectName(QStringLiteral("StageDisplay"));
    setWindowTitle(QStringLiteral("MxU Slides — Stage Display"));
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setWindowFlags(Qt::Window | Qt::WindowDoesNotAcceptFocus);

    m_clock = new QTimer(this);
    m_clock->setInterval(500);
    connect(m_clock, &QTimer::timeout, this, [this] {
        if (m_active)
            update();
    });

    connect(editor, &Editor::presentationChanged, this, &StageWindow::refresh);
    connect(editor, &Editor::presentationEdited, this, &StageWindow::refresh);
    connect(editor, &Editor::liveChanged, this, &StageWindow::refresh);
    connect(editor, &Editor::slideIndexChanged, this, &StageWindow::refresh);
    connect(editor, &Editor::timerChanged, this, &StageWindow::refresh);
}

void StageWindow::setTargetScreen(QScreen *screen)
{
    if (m_screen == screen)
        return;
    m_screen = screen;
    if (m_active)
        configureWindow();
}

void StageWindow::configureWindow()
{
    Qt::WindowFlags flags = Qt::Window | Qt::WindowDoesNotAcceptFocus;
    if (m_screen)
        flags |= Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint;
    setWindowFlags(flags);
    setAttribute(Qt::WA_ShowWithoutActivating, true);

    if (m_screen) {
        if (windowHandle())
            windowHandle()->setScreen(m_screen);
        setCursor(Qt::BlankCursor);
        setGeometry(m_screen->geometry());
        showFullScreen();
    } else {
        setCursor(Qt::ArrowCursor);
        resize(1000, 620);
        showNormal();
        show();
    }
    raise();
}

void StageWindow::setActive(bool active)
{
    if (m_active == active) {
        if (active)
            update();
        return;
    }
    m_active = active;
    if (!active) {
        m_clock->stop();
        hide();
        return;
    }
    configureWindow();
    m_clock->start();
    update();
}

void StageWindow::refresh()
{
    if (m_active)
        update();
}

void StageWindow::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const Palette &p = mxu::ui::palette();
    const qreal width = this->width();
    const qreal height = this->height();
    if (width <= 0 || height <= 0)
        return;

    QLinearGradient background(0, 0, 0, height);
    background.setColorAt(0.0, QColor(10, 12, 15));
    background.setColorAt(1.0, QColor(18, 21, 26));
    painter.fillRect(rect(), background);

    const qreal margin = width * 0.035;
    const qreal headerHeight = height * 0.105;
    const qreal footerHeight = height * 0.16;
    const Theme theme = m_editor->theme();
    const bool hasDeck = m_editor->hasPresentation()
        && !m_editor->presentation().slides.isEmpty();
    const int count = hasDeck ? m_editor->presentation().slides.size() : 0;
    int live = m_editor->liveIndex() >= 0 ? m_editor->liveIndex() : m_editor->currentIndex();
    live = count > 0 ? qBound(0, live, count - 1) : -1;

    const int markSide = int(headerHeight * 0.55);
    paintAppMark(painter, QRectF(margin, (headerHeight - markSide) / 2.0, markSide, markSide));
    QFont brandFont = uiFont(10, QFont::DemiBold);
    brandFont.setPixelSize(int(headerHeight * 0.19));
    brandFont.setLetterSpacing(QFont::AbsoluteSpacing, 1.5);
    painter.setFont(brandFont);
    painter.setPen(p.textMuted);
    painter.drawText(QRectF(margin + markSide + 10, 0, width * 0.22, headerHeight),
                     Qt::AlignVCenter | Qt::AlignLeft, QStringLiteral("STAGE DISPLAY"));

    QFont titleFont = uiFont(10, QFont::DemiBold);
    titleFont.setPixelSize(int(headerHeight * 0.22));
    painter.setFont(titleFont);
    painter.setPen(p.text);
    const QString title = hasDeck ? m_editor->presentation().name : QStringLiteral("No presentation");
    painter.drawText(QRectF(width * 0.30, 0, width * 0.40, headerHeight),
                     Qt::AlignCenter, painter.fontMetrics().elidedText(title, Qt::ElideMiddle,
                                                                       int(width * 0.38)));

    QFont clockFont = uiFont(10, QFont::DemiBold);
    clockFont.setPixelSize(int(headerHeight * 0.24));
    painter.setFont(clockFont);
    painter.setPen(p.textMuted);
    painter.drawText(QRectF(width - margin - width * 0.22, 0, width * 0.22, headerHeight),
                     Qt::AlignRight | Qt::AlignVCenter,
                     QTime::currentTime().toString(QStringLiteral("h:mm:ss AP")));

    const qreal contentTop = headerHeight + height * 0.02;
    const qreal contentBottom = height - footerHeight - height * 0.02;
    const qreal contentHeight = contentBottom - contentTop;
    painter.setPen(QPen(p.border, 1));
    painter.drawLine(QPointF(margin, headerHeight), QPointF(width - margin, headerHeight));

    const qreal leftWidth = (width - margin * 2) * 0.60;
    const qreal rightX = margin + leftWidth + width * 0.025;
    painter.drawLine(QPointF(rightX - width * 0.012, contentTop),
                     QPointF(rightX - width * 0.012, contentBottom));

    auto caption = [&](const QString &text, const QRectF &at, const QColor &color) {
        QFont font = uiFont(8, QFont::Bold);
        font.setPixelSize(qMax(9, int(height * 0.016)));
        font.setLetterSpacing(QFont::AbsoluteSpacing, 1.3);
        painter.setFont(font);
        painter.setPen(color);
        painter.drawText(at, Qt::AlignLeft | Qt::AlignVCenter, text);
    };

    if (live >= 0) {
        const Slide &slide = m_editor->presentation().slides.at(live);
        const QString current = slideText(slide);
        caption(QStringLiteral("CURRENT"), QRectF(margin, contentTop, leftWidth, height * 0.03),
                p.accent);
        drawFittedText(painter,
                       QRectF(margin, contentTop + height * 0.045, leftWidth,
                              contentHeight - height * 0.10),
                       current, p.text, int(height * 0.075), 20);

        caption(QStringLiteral("SLIDE %1 / %2").arg(live + 1).arg(count),
                QRectF(margin, contentBottom - height * 0.045, leftWidth, height * 0.035),
                p.textFaint);

        const qreal nextTop = contentTop + height * 0.20;
        const qreal nextHeight = contentHeight * 0.34;
        if (live + 1 < count) {
            caption(QStringLiteral("NEXT"), QRectF(rightX, contentTop, width - margin - rightX,
                                                   height * 0.03),
                    p.textMuted);
            drawFittedText(painter,
                           QRectF(rightX, nextTop, width - margin - rightX, nextHeight),
                           slideText(m_editor->presentation().slides.at(live + 1)),
                           p.textMuted, int(height * 0.033), 14, QFont::Normal);
        } else {
            caption(QStringLiteral("NEXT"), QRectF(rightX, contentTop, width - margin - rightX,
                                                   height * 0.03),
                    p.textMuted);
            drawFittedText(painter, QRectF(rightX, nextTop, width - margin - rightX, nextHeight),
                           QStringLiteral("End of presentation"), p.textFaint,
                           int(height * 0.028), 14, QFont::Normal);
        }

        const qreal notesTop = contentTop + contentHeight * 0.58;
        caption(QStringLiteral("NOTES"), QRectF(rightX, notesTop, width - margin - rightX,
                                               height * 0.03),
                p.textMuted);
        drawFittedText(painter,
                       QRectF(rightX, notesTop + height * 0.04, width - margin - rightX,
                              contentBottom - notesTop - height * 0.04),
                       slide.notes.isEmpty() ? QStringLiteral("No notes") : slide.notes,
                       p.textMuted, int(height * 0.024), 12, QFont::Normal);
    } else {
        drawFittedText(painter, QRectF(margin, contentTop, width - margin * 2,
                                      contentBottom - contentTop),
                       QStringLiteral("Waiting for a presentation"), p.textFaint,
                       int(height * 0.06), 20);
    }

    painter.setPen(QPen(p.border, 1));
    painter.drawLine(QPointF(margin, height - footerHeight),
                     QPointF(width - margin, height - footerHeight));

    const int remaining = m_editor->timerRemainingSeconds();
    QFont timerFont = uiFont(9, QFont::DemiBold);
    timerFont.setPixelSize(int(footerHeight * 0.44));
    painter.setFont(timerFont);
    const bool urgent = m_editor->timerRunning() && remaining <= 30;
    painter.setPen(urgent ? p.live : p.text);
    painter.drawText(QRectF(margin, height - footerHeight, width * 0.34, footerHeight),
                     Qt::AlignLeft | Qt::AlignVCenter, formatTimer(remaining));
    QFont timerCaption = uiFont(8, QFont::Bold);
    timerCaption.setPixelSize(qMax(9, int(height * 0.015)));
    painter.setFont(timerCaption);
    painter.setPen(p.textFaint);
    painter.drawText(QRectF(margin, height - footerHeight * 0.42, width * 0.28,
                            footerHeight * 0.3),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     m_editor->timerRunning() ? QStringLiteral("TIMER RUNNING")
                                              : QStringLiteral("TIMER PAUSED"));

    if (m_editor->blackout()) {
        const QString state = QStringLiteral("BLACKOUT ACTIVE");
        QFont font = uiFont(8, QFont::Bold);
        font.setPixelSize(qMax(10, int(height * 0.019)));
        font.setLetterSpacing(QFont::AbsoluteSpacing, 1.2);
        painter.setFont(font);
        const QFontMetricsF metrics(font);
        const QRectF badge(width / 2.0 - (metrics.horizontalAdvance(state) + 30) / 2.0,
                           height - footerHeight + (footerHeight - 34) / 2.0,
                           metrics.horizontalAdvance(state) + 30, 34);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(255, 91, 86, 40));
        painter.drawRoundedRect(badge, 7, 7);
        painter.setPen(p.live);
        painter.drawText(badge, Qt::AlignCenter, state);
    } else {
        QFont status = uiFont(9, QFont::Normal);
        status.setPixelSize(qMax(10, int(height * 0.020)));
        painter.setFont(status);
        painter.setPen(p.textMuted);
        painter.drawText(QRectF(width / 2.0, height - footerHeight, width * 0.30, footerHeight),
                         Qt::AlignCenter,
                         hasDeck ? m_editor->presentation().name : QStringLiteral("No deck"));
    }

    QFont rightFont = uiFont(8, QFont::Normal);
    rightFont.setPixelSize(qMax(9, int(height * 0.017)));
    painter.setFont(rightFont);
    painter.setPen(p.textFaint);
    painter.drawText(QRectF(width - margin - width * 0.25, height - footerHeight,
                            width * 0.25, footerHeight),
                     Qt::AlignRight | Qt::AlignVCenter,
                     QStringLiteral("Qt 6.12 · Windows"));
}

void StageWindow::closeEvent(QCloseEvent *event)
{
    m_active = false;
    m_clock->stop();
    hide();
    event->accept();
}

} // namespace mxu::ui
