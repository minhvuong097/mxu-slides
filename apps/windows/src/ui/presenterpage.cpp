#include "ui/presenterpage.h"

#include "core/editor.h"
#include "render/sliderenderer.h"
#include "ui/icons.h"
#include "ui/theme.h"
#include "ui/widgets.h"

#include <QDateTime>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTime>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace mxu::ui {

class PreviewWidget : public QWidget {
public:
    PreviewWidget(Editor *editor, SlideRenderer *renderer, bool next, QWidget *parent = nullptr)
        : QWidget(parent)
        , m_editor(editor)
        , m_renderer(renderer)
        , m_next(next)
    {
        setMinimumSize(m_next ? QSize(280, 158) : QSize(480, 270));
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const Palette &p = mxu::ui::palette();
        QPainterPath clip;
        clip.addRoundedRect(QRectF(rect()).adjusted(1, 1, -1, -1), 10, 10);
        painter.setClipPath(clip);
        painter.fillRect(rect(), QColor(7, 9, 12));

        const bool hasDeck = m_editor->hasPresentation()
            && !m_editor->presentation().slides.isEmpty();
        const int count = hasDeck ? m_editor->presentation().slides.size() : 0;
        int live = m_editor->liveIndex() >= 0 ? m_editor->liveIndex() : m_editor->currentIndex();
        if (!hasDeck || count == 0) {
            painter.setClipping(false);
            painter.setPen(p.textFaint);
            painter.setFont(uiFont(10));
            painter.drawText(rect(), Qt::AlignCenter,
                             m_next ? QStringLiteral("No next slide") : QStringLiteral("No presentation"));
            drawChrome(painter, false);
            return;
        }

        live = qBound(0, live, count - 1);
        int index = m_next ? live + 1 : live;
        const bool ended = m_next && index >= count;
        if (ended) {
            painter.setClipping(false);
            painter.setPen(p.textMuted);
            painter.setFont(uiFont(11));
            painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("End of presentation"));
            drawChrome(painter, false);
            return;
        }
        index = qBound(0, index, count - 1);

        if (!m_next && m_editor->blackout()) {
            painter.fillRect(rect(), Qt::black);
            drawChrome(painter, true);
            return;
        }
        if (!m_next && m_editor->logoVisible()) {
            painter.fillRect(rect(), m_editor->theme().backgroundColor());
            const int side = qMin(width(), height()) / 3;
            paintAppMark(painter, QRectF(width() / 2.0 - side / 2.0,
                                         height() / 2.0 - side / 2.0, side, side));
            drawChrome(painter, true);
            return;
        }

        RenderContext context;
        context.clock = QTime::currentTime();
        context.timerSeconds = m_editor->timerRemainingSeconds();
        context.slideIndex = index;
        context.slideCount = count;
        context.presentationName = m_editor->presentation().name;
        const QImage image = m_renderer->render(m_editor->presentation(),
                                                m_editor->presentation().slides.at(index),
                                                m_editor->theme(), QSize(1280, 720), context);
        if (!image.isNull()) {
            const QSizeF scaled = image.size().scaled(size(), Qt::KeepAspectRatio);
            const QRectF target((width() - scaled.width()) / 2.0,
                                (height() - scaled.height()) / 2.0, scaled.width(),
                                scaled.height());
            painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
            painter.drawImage(target, image);
        }

        painter.setClipping(false);
        drawChrome(painter, !m_next && m_editor->isPresenting());
    }

private:
    void drawChrome(QPainter &painter, bool liveFrame)
    {
        const Palette &p = mxu::ui::palette();
        painter.setClipping(false);
        const QRectF outer = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(liveFrame ? p.live : p.border, liveFrame ? 1.4 : 1.0));
        painter.drawRoundedRect(outer, 10, 10);

        const QString label = m_next ? QStringLiteral("NEXT") : QStringLiteral("PROGRAM");
        QFont font = uiFont(7, QFont::Bold);
        font.setLetterSpacing(QFont::AbsoluteSpacing, 1.0);
        painter.setFont(font);
        const QFontMetrics metrics(font);
        const QRectF badge(14, 14, metrics.horizontalAdvance(label) + 18, 21);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 170));
        painter.drawRoundedRect(badge, 5, 5);
        painter.setPen(Qt::white);
        painter.drawText(badge, Qt::AlignCenter, label);
    }

    Editor *m_editor = nullptr;
    SlideRenderer *m_renderer = nullptr;
    bool m_next = false;
};

PresenterPage::PresenterPage(Editor *editor, SlideRenderer *renderer, QWidget *parent)
    : QWidget(parent)
    , m_editor(editor)
    , m_renderer(renderer)
{
    setObjectName(QStringLiteral("PresenterPage"));
    createUi();

    m_tick = new QTimer(this);
    m_tick->setInterval(500);
    connect(m_tick, &QTimer::timeout, this, &PresenterPage::updateClockAndTimer);
    m_tick->start();

    connect(editor, &Editor::presentationChanged, this, &PresenterPage::refresh);
    connect(editor, &Editor::presentationEdited, this, &PresenterPage::refresh);
    connect(editor, &Editor::slideIndexChanged, this, &PresenterPage::refresh);
    connect(editor, &Editor::liveChanged, this, &PresenterPage::refresh);
    connect(editor, &Editor::modeChanged, this, &PresenterPage::refresh);
    connect(editor, &Editor::timerChanged, this, &PresenterPage::refresh);
    refresh();
}

void PresenterPage::createUi()
{
    const Palette &p = mxu::ui::palette();
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 16, 20, 16);
    root->setSpacing(14);

    auto *top = new QHBoxLayout;
    top->setSpacing(10);
    auto *titles = new QVBoxLayout;
    titles->setSpacing(2);
    titles->addWidget(makeLabel(QStringLiteral("PRESENTATION"), QStringLiteral("SectionTitle")));
    m_title = makeLabel(QStringLiteral("Presenter view"), QStringLiteral("Title"));
    titles->addWidget(m_title);
    m_position = makeLabel(QStringLiteral("Slide 1 of 1"), QStringLiteral("Muted"));
    titles->addWidget(m_position);
    top->addLayout(titles);

    auto *live = makeLabel(QStringLiteral("●  LIVE"), QStringLiteral("Chip"));
    live->setStyleSheet(QStringLiteral(
                            "QLabel { color: %1; background: rgba(255, 91, 86, 24); "
                            "border: 1px solid rgba(255, 91, 86, 70); border-radius: 8px; "
                            "padding: 4px 9px; font-weight: 600; font-size: 8pt; }")
                            .arg(p.live.name()));
    top->addWidget(live, 0, Qt::AlignTop);
    top->addStretch();

    auto *clockBlock = new QVBoxLayout;
    clockBlock->setSpacing(0);
    auto *clockCaption = makeLabel(QStringLiteral("LOCAL TIME"), QStringLiteral("SectionTitle"));
    clockCaption->setAlignment(Qt::AlignRight);
    clockBlock->addWidget(clockCaption);
    m_clock = makeLabel(QStringLiteral("--:--:--"), QStringLiteral("Muted"));
    m_clock->setFont(uiFont(16, QFont::DemiBold));
    m_clock->setAlignment(Qt::AlignRight);
    clockBlock->addWidget(m_clock);
    top->addLayout(clockBlock);
    root->addLayout(top);

    auto *main = new QHBoxLayout;
    main->setSpacing(14);

    auto *left = new QVBoxLayout;
    left->setSpacing(7);
    auto *programHeader = new QHBoxLayout;
    programHeader->addWidget(makeLabel(QStringLiteral("PROGRAM OUTPUT"),
                                       QStringLiteral("SectionTitle")));
    programHeader->addStretch();
    m_outputSettings = new QPushButton(QStringLiteral("Output settings"));
    m_outputSettings->setObjectName(QStringLiteral("Ghost"));
    programHeader->addWidget(m_outputSettings);
    left->addLayout(programHeader);
    m_program = new PreviewWidget(m_editor, m_renderer, false, this);
    left->addWidget(m_program, 1);
    main->addLayout(left, 1);

    auto *rightHost = new QWidget(this);
    rightHost->setFixedWidth(330);
    auto *right = new QVBoxLayout(rightHost);
    right->setContentsMargins(0, 0, 0, 0);
    right->setSpacing(9);

    auto *nextHeader = new QHBoxLayout;
    nextHeader->addWidget(makeLabel(QStringLiteral("NEXT"), QStringLiteral("SectionTitle")));
    nextHeader->addStretch();
    m_nextLabel = makeLabel(QString(), QStringLiteral("Faint"));
    nextHeader->addWidget(m_nextLabel);
    right->addLayout(nextHeader);
    m_next = new PreviewWidget(m_editor, m_renderer, true, rightHost);
    m_next->setFixedHeight(174);
    right->addWidget(m_next);

    auto *timerCard = new SectionCard(QStringLiteral("TIMER"), rightHost);
    auto *timerRow = new QHBoxLayout;
    timerRow->setSpacing(6);
    m_timerValue = makeLabel(QStringLiteral("05:00"));
    m_timerValue->setFont(uiFont(22, QFont::DemiBold));
    timerRow->addWidget(m_timerValue);
    timerRow->addStretch();
    m_timerToggle = makeIconButton(Glyph::Play, QStringLiteral("Start or pause the timer"),
                                   timerCard);
    m_timerReset = makeIconButton(Glyph::Reset, QStringLiteral("Reset the timer"), timerCard);
    timerRow->addWidget(m_timerToggle);
    timerRow->addWidget(m_timerReset);
    timerCard->body()->addLayout(timerRow);
    right->addWidget(timerCard);

    auto *notesCard = new SectionCard(QStringLiteral("SPEAKER NOTES"), rightHost);
    m_notes = new QPlainTextEdit;
    m_notes->setReadOnly(true);
    m_notes->setPlaceholderText(QStringLiteral("No notes for this slide."));
    m_notes->setMinimumHeight(120);
    notesCard->body()->addWidget(m_notes);
    right->addWidget(notesCard, 1);
    main->addWidget(rightHost);
    root->addLayout(main, 1);

    auto *transport = new QFrame(this);
    transport->setObjectName(QStringLiteral("Card"));
    auto *transportLayout = new QHBoxLayout(transport);
    transportLayout->setContentsMargins(10, 8, 10, 8);
    transportLayout->setSpacing(7);
    transportLayout->addStretch();

    m_previous = new QPushButton(QStringLiteral("Previous"));
    m_previous->setIcon(ui::icon(Glyph::ArrowLeft, p.textMuted, 16));
    m_nextButton = new QPushButton(QStringLiteral("Next slide"));
    m_nextButton->setObjectName(QStringLiteral("Accent"));
    m_nextButton->setIcon(ui::icon(Glyph::ArrowRight, Qt::white, 16));
    m_blackout = new QPushButton(QStringLiteral("Blackout"));
    m_blackout->setCheckable(true);
    m_blackout->setIcon(ui::icon(Glyph::Blackout, p.textMuted, 16));
    m_blackout->setStyleSheet(QStringLiteral(
                                  "QPushButton:checked { background: rgba(255, 91, 86, 35); "
                                  "border-color: %1; color: %1; }")
                                  .arg(p.live.name()));
    m_logo = new QPushButton(QStringLiteral("Logo"));
    m_logo->setCheckable(true);
    m_logo->setIcon(ui::icon(Glyph::Logo, p.textMuted, 16));
    m_logo->setStyleSheet(QStringLiteral(
                              "QPushButton:checked { background: %1; border-color: %1; color: white; }")
                              .arg(p.accentSoft.name()));
    m_stop = new QPushButton(QStringLiteral("Stop"));
    m_stop->setObjectName(QStringLiteral("Danger"));
    m_stop->setIcon(ui::icon(Glyph::Stop, p.live, 16));

    transportLayout->addWidget(m_previous);
    transportLayout->addWidget(m_nextButton);
    transportLayout->addSpacing(10);
    transportLayout->addWidget(m_blackout);
    transportLayout->addWidget(m_logo);
    transportLayout->addWidget(m_stop);
    transportLayout->addStretch();
    root->addWidget(transport);

    connect(m_previous, &QPushButton::clicked, m_editor, &Editor::previous);
    connect(m_nextButton, &QPushButton::clicked, m_editor, &Editor::next);
    connect(m_stop, &QPushButton::clicked, m_editor, &Editor::stopPresenting);
    connect(m_blackout, &QPushButton::toggled, m_editor, &Editor::setBlackout);
    connect(m_logo, &QPushButton::toggled, m_editor, &Editor::setLogoVisible);
    connect(m_timerToggle, &QToolButton::clicked, this, [this] {
        if (m_editor->timerRunning())
            m_editor->pauseTimer();
        else
            m_editor->startTimer();
    });
    connect(m_timerReset, &QToolButton::clicked, m_editor, &Editor::resetTimer);
    connect(m_outputSettings, &QPushButton::clicked, this,
            &PresenterPage::outputSettingsRequested);
}

void PresenterPage::refresh()
{
    const bool hasDeck = m_editor->hasPresentation();
    const auto &deck = m_editor->presentation();
    const int count = hasDeck ? deck.slides.size() : 0;
    int index = m_editor->liveIndex() >= 0 ? m_editor->liveIndex() : m_editor->currentIndex();
    index = count > 0 ? qBound(0, index, count - 1) : -1;

    m_title->setText(hasDeck ? deck.name : QStringLiteral("No presentation open"));
    m_position->setText(count > 0 ? QStringLiteral("Slide %1 of %2").arg(index + 1).arg(count)
                                 : QStringLiteral("No slides"));
    if (count > 0) {
        const int nextIndex = index + 1;
        if (nextIndex < count)
            m_nextLabel->setText(deck.slides.at(nextIndex).name);
        else
            m_nextLabel->setText(QStringLiteral("End of deck"));
        const Slide &slide = deck.slides.at(index);
        m_notes->setPlainText(slide.notes.isEmpty()
                                  ? QStringLiteral("No notes for this slide.")
                                  : slide.notes);
    } else {
        m_nextLabel->clear();
        m_notes->setPlainText(QStringLiteral("Open a presentation to see speaker notes."));
    }

    {
        QSignalBlocker blocker(m_blackout);
        m_blackout->setChecked(m_editor->blackout());
    }
    {
        QSignalBlocker blocker(m_logo);
        m_logo->setChecked(m_editor->logoVisible());
    }
    m_timerToggle->setIcon(ui::icon(m_editor->timerRunning() ? Glyph::Pause : Glyph::Play,
                                    mxu::ui::palette().textMuted, 17));
    m_timerValue->setText(ui::formatTimer(m_editor->timerRemainingSeconds()));
    m_timerValue->setStyleSheet(
        m_editor->timerRunning() && m_editor->timerRemainingSeconds() <= 30
            ? QStringLiteral("color: %1;").arg(mxu::ui::palette().live.name())
            : QString());
    m_previous->setEnabled(count > 0);
    m_nextButton->setEnabled(count > 0);
    m_blackout->setEnabled(count > 0);
    m_logo->setEnabled(count > 0);
    m_stop->setEnabled(m_editor->isPresenting());
    m_outputSettings->setEnabled(true);

    m_clock->setText(QTime::currentTime().toString(QStringLiteral("h:mm:ss AP")));
    m_program->update();
    m_next->update();
}

void PresenterPage::updateClockAndTimer()
{
    m_clock->setText(QTime::currentTime().toString(QStringLiteral("h:mm:ss AP")));
    m_timerValue->setText(ui::formatTimer(m_editor->timerRemainingSeconds()));
    m_timerValue->setStyleSheet(
        m_editor->timerRunning() && m_editor->timerRemainingSeconds() <= 30
            ? QStringLiteral("color: %1;").arg(mxu::ui::palette().live.name())
            : QString());
    m_program->update();
    m_next->update();
}

} // namespace mxu::ui
