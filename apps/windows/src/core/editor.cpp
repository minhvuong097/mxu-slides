#include "core/editor.h"

#include "core/library.h"
#include "core/presentationfile.h"

#include <QDateTime>
#include <QFileInfo>
#include <QJsonArray>
#include <QSet>

#include <algorithm>

namespace mxu {
namespace {

QString formatTimer(int seconds)
{
    seconds = std::max(0, seconds);
    const int minutes = seconds / 60;
    const int remainder = seconds % 60;
    if (minutes >= 60) {
        const int hours = minutes / 60;
        return QStringLiteral("%1:%2:%3")
            .arg(hours, 2, 10, QLatin1Char('0'))
            .arg(minutes % 60, 2, 10, QLatin1Char('0'))
            .arg(remainder, 2, 10, QLatin1Char('0'));
    }
    return QStringLiteral("%1:%2")
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(remainder, 2, 10, QLatin1Char('0'));
}

} // namespace

Editor::Editor(Library *library, QObject *parent)
    : QObject(parent)
    , m_library(library)
    , m_autosave(new QTimer(this))
    , m_timerTick(new QTimer(this))
{
    m_autosave->setSingleShot(true);
    m_autosave->setInterval(450);
    connect(m_autosave, &QTimer::timeout, this, &Editor::autosave);

    m_timerTick->setInterval(200);
    connect(m_timerTick, &QTimer::timeout, this, [this] {
        if (!m_timerRunning)
            return;
        if (timerRemainingMs() <= 0) {
            m_timerRunning = false;
            m_pausedRemainingMs = 0;
            m_timerTick->stop();
        }
        emit timerChanged();
    });

    m_timerTotalSeconds = 300;
    m_pausedRemainingMs = m_timerTotalSeconds * 1000LL;
    m_theme = defaultTheme();
}

const Slide *Editor::currentSlide() const
{
    if (!m_hasPresentation || m_currentIndex < 0 || m_currentIndex >= m_presentation.slides.size())
        return nullptr;
    return &m_presentation.slides.at(m_currentIndex);
}

Slide *Editor::currentSlideMutable()
{
    if (!m_hasPresentation || m_currentIndex < 0 || m_currentIndex >= m_presentation.slides.size())
        return nullptr;
    return &m_presentation.slides[m_currentIndex];
}

const SlideObject *Editor::selectedObject() const
{
    const Slide *slide = currentSlide();
    if (!slide || m_selectedObjectId.isEmpty())
        return nullptr;
    const int index = slide->indexOfObject(m_selectedObjectId);
    return index >= 0 ? &slide->objects.at(index) : nullptr;
}

Theme Editor::theme() const
{
    for (const Theme &candidate : m_themes) {
        if (candidate.id == m_presentation.themeId)
            return candidate;
    }
    if (!m_themes.isEmpty())
        return m_themes.first();
    return defaultTheme();
}

QString Editor::mediaPath(const QString &mediaId) const
{
    return m_mediaPaths.value(mediaId);
}

bool Editor::open(const QString &presentationId, QString *error)
{
    if (m_hasPresentation && m_dirty)
        save();
    Presentation loaded;
    if (!m_library->loadPresentation(presentationId, &loaded, error))
        return false;

    m_presentation = loaded;
    m_hasPresentation = true;
    m_currentIndex = 0;
    m_liveIndex = -1;
    m_mode = Mode::Edit;
    m_blackout = false;
    m_logo = false;
    m_selectedObjectId.clear();
    m_undo.clear();
    m_redo.clear();
    m_interactive = false;
    m_dirty = false;
    m_revision++;

    reloadThemes();
    reloadMedia();
    ensureThemeAvailable();
    pruneSelection();

    emit presentationChanged();
    emit slideIndexChanged(m_currentIndex);
    emit selectionChanged(QString());
    emit modeChanged();
    emit liveChanged();
    emit dirtyChanged(false);
    emit undoStateChanged();
    emit statusMessage(QStringLiteral("Opened “%1”").arg(m_presentation.name), 2500);
    return true;
}

void Editor::createPresentation(const QString &name)
{
    if (m_hasPresentation && m_dirty)
        save();
    reloadThemes();
    ensureThemeAvailable();

    QString chosenName = name.trimmed();
    if (chosenName.isEmpty())
        chosenName = QStringLiteral("Untitled");
    chosenName = m_library->uniquePresentationName(chosenName);

    Presentation deck = makePresentation(chosenName, theme());
    Slide &first = deck.slides.first();
    SlideObject title = makeTextObject(QStringLiteral("Title"), chosenName,
                                       QRectF(180, 340, 1560, 280), 124);
    title.textStyle.horizontalAlignment = QStringLiteral("center");
    first.objects.append(title);
    SlideObject subtitle = makeTextObject(QStringLiteral("Subtitle"), QStringLiteral("MxU Slides"),
                                          QRectF(180, 640, 1560, 100), 38);
    subtitle.textStyle.colorSet = true;
    subtitle.textStyle.color = QColor(150, 163, 182);
    first.objects.append(subtitle);

    m_presentation = deck;
    m_hasPresentation = true;
    m_currentIndex = 0;
    m_liveIndex = -1;
    m_mode = Mode::Edit;
    m_blackout = false;
    m_logo = false;
    m_selectedObjectId.clear();
    m_undo.clear();
    m_redo.clear();
    m_revision++;
    m_dirty = true;

    QString saveError;
    if (m_library->savePresentation(m_presentation, &saveError)) {
        m_dirty = false;
        emit libraryChanged();
    } else {
        emit statusMessage(saveError, 6000);
    }

    emit presentationChanged();
    emit slideIndexChanged(m_currentIndex);
    emit selectionChanged(QString());
    emit modeChanged();
    emit liveChanged();
    emit dirtyChanged(m_dirty);
    emit undoStateChanged();
    emit libraryChanged();
    emit statusMessage(QStringLiteral("Created “%1”").arg(chosenName), 2500);
}

void Editor::close()
{
    if (m_hasPresentation && m_dirty)
        save();
    m_autosave->stop();
    m_presentation = Presentation();
    m_hasPresentation = false;
    m_currentIndex = 0;
    m_liveIndex = -1;
    m_mode = Mode::Edit;
    m_selectedObjectId.clear();
    m_undo.clear();
    m_redo.clear();
    m_dirty = false;
    m_revision++;
    emit presentationChanged();
    emit selectionChanged(QString());
    emit modeChanged();
    emit liveChanged();
    emit dirtyChanged(false);
    emit undoStateChanged();
}

bool Editor::save(QString *error)
{
    if (!m_hasPresentation) {
        if (error)
            *error = QStringLiteral("There is no open presentation to save.");
        return false;
    }
    m_autosave->stop();
    if (!m_library->savePresentation(m_presentation, error))
        return false;
    if (m_dirty) {
        m_dirty = false;
        emit dirtyChanged(false);
    }
    emit libraryChanged();
    emit statusMessage(QStringLiteral("Saved"), 1600);
    return true;
}

void Editor::flushAutosave()
{
    m_autosave->stop();
    if (m_hasPresentation && m_dirty)
        autosave();
}

void Editor::scheduleAutosave()
{
    if (!m_hasPresentation)
        return;
    m_autosave->start();
}

void Editor::autosave()
{
    if (!m_hasPresentation || !m_dirty)
        return;
    QString error;
    if (!m_library->savePresentation(m_presentation, &error)) {
        emit statusMessage(QStringLiteral("Autosave failed: %1").arg(error), 6000);
        return;
    }
    m_dirty = false;
    emit dirtyChanged(false);
    emit libraryChanged();
}

bool Editor::exportBundle(const QString &path, QString *error)
{
    if (!m_hasPresentation) {
        if (error)
            *error = QStringLiteral("Open a presentation before exporting.");
        return false;
    }
    QString target = path;
    if (!target.endsWith(QStringLiteral(".mxuslides"), Qt::CaseInsensitive))
        target += QStringLiteral(".mxuslides");
    QStringList warnings;
    const Bundle bundle = m_library->bundleFor(m_presentation, &warnings);
    if (!PresentationFile::write(bundle, target, error))
        return false;
    if (!warnings.isEmpty())
        emit statusMessage(QStringLiteral("Exported with %1 missing media item(s).").arg(warnings.size()), 5000);
    else
        emit statusMessage(QStringLiteral("Exported “%1”").arg(QFileInfo(target).fileName()), 3000);
    return true;
}

bool Editor::importBundle(const QString &path, QString *error)
{
    QString newId;
    QStringList warnings;
    if (!m_library->importBundle(path, &newId, error, &warnings))
        return false;
    emit libraryChanged();
    if (!newId.isEmpty())
        open(newId, error);
    if (!warnings.isEmpty()) {
        emit statusMessage(QStringLiteral("Imported with %1 warning(s).").arg(warnings.size()), 5000);
    } else {
        emit statusMessage(QStringLiteral("Imported “%1”").arg(QFileInfo(path).fileName()), 3000);
    }
    return true;
}

QString Editor::importMediaFile(const QString &sourcePath, QString *error, QString *warning)
{
    if (!warning)
        warning = error;
    MediaItem item;
    QString importError;
    const QString id = m_library->importMedia(sourcePath, QFileInfo(sourcePath).completeBaseName(), &item,
                                              &importError);
    if (id.isEmpty()) {
        if (error)
            *error = importError;
        return {};
    }
    reloadMedia();
    return id;
}

void Editor::setCurrentIndex(int index)
{
    if (!m_hasPresentation || m_presentation.slides.isEmpty())
        return;
    index = std::clamp(index, 0, slideCount() - 1);
    if (index != m_currentIndex) {
        m_currentIndex = index;
        m_selectedObjectId.clear();
        emit selectionChanged(QString());
        emit slideIndexChanged(m_currentIndex);
    }
    if (m_mode == Mode::Present && m_liveIndex != m_currentIndex)
        goLive(m_currentIndex);
}

void Editor::addSlide()
{
    if (!m_hasPresentation)
        return;
    commit(
        QStringLiteral("Add slide"),
        [this](Presentation &presentation) {
            Slide slide = makeBlankSlide(static_cast<int>(presentation.slides.size()) + 1);
            slide.name = uniqueSlideName();
            const int insertAt = std::clamp(m_currentIndex + 1, 0,
                                            static_cast<int>(presentation.slides.size()));
            presentation.slides.insert(insertAt, slide);
            m_currentIndex = insertAt;
            m_selectedObjectId.clear();
        },
        true);
    emit slideIndexChanged(m_currentIndex);
    emit selectionChanged(QString());
}

void Editor::duplicateSlide()
{
    if (!m_hasPresentation || !currentSlide())
        return;
    commit(
        QStringLiteral("Duplicate slide"),
        [this](Presentation &presentation) {
            Slide copy = presentation.slides.at(m_currentIndex);
            copy.id = newId();
            copy.name = QStringLiteral("%1 copy").arg(copy.name);
            for (SlideObject &object : copy.objects)
                object.id = newId();
            const int insertAt = std::clamp(m_currentIndex + 1, 0,
                                            static_cast<int>(presentation.slides.size()));
            presentation.slides.insert(insertAt, copy);
            m_currentIndex = insertAt;
            m_selectedObjectId.clear();
        },
        true);
    emit slideIndexChanged(m_currentIndex);
    emit selectionChanged(QString());
}

void Editor::deleteCurrentSlide()
{
    if (!m_hasPresentation || m_presentation.slides.isEmpty())
        return;
    commit(
        QStringLiteral("Delete slide"),
        [this](Presentation &presentation) {
            presentation.slides.removeAt(m_currentIndex);
            if (presentation.slides.isEmpty())
                presentation.slides.append(makeBlankSlide(1));
            m_currentIndex = std::clamp(m_currentIndex, 0,
                                        static_cast<int>(presentation.slides.size()) - 1);
            m_selectedObjectId.clear();
        },
        true);
    if (m_liveIndex >= slideCount())
        m_liveIndex = slideCount() - 1;
    emit slideIndexChanged(m_currentIndex);
    emit selectionChanged(QString());
    emit liveChanged();
}

void Editor::moveSlide(int from, int to)
{
    if (!m_hasPresentation)
        return;
    if (from < 0 || from >= slideCount() || to < 0 || to >= slideCount() || from == to) {
        return;
    }
    commit(
        QStringLiteral("Move slide"),
        [from, to](Presentation &presentation) { presentation.slides.move(from, to); },
        true);
    m_currentIndex = to;
    emit slideIndexChanged(m_currentIndex);
}

void Editor::selectObject(const QString &objectId)
{
    if (m_selectedObjectId == objectId)
        return;
    const Slide *slide = currentSlide();
    if (!objectId.isEmpty() && (!slide || slide->indexOfObject(objectId) < 0))
        return;
    m_selectedObjectId = objectId;
    emit selectionChanged(m_selectedObjectId);
}

void Editor::addObject(const SlideObject &object)
{
    if (!m_hasPresentation || !currentSlide())
        return;
    SlideObject copy = object;
    if (copy.id.isEmpty())
        copy.id = newId();
    const QString id = copy.id;
    commit(
        QStringLiteral("Add %1").arg(object.isText() ? QStringLiteral("text") : object.isShape()
                                                                              ? QStringLiteral("shape")
                                                                              : QStringLiteral("object")),
        [&copy, this](Presentation &presentation) {
            presentation.slides[m_currentIndex].objects.append(copy);
        });
    m_selectedObjectId = id;
    emit selectionChanged(m_selectedObjectId);
}

void Editor::deleteSelectedObject()
{
    if (m_selectedObjectId.isEmpty() || !currentSlide())
        return;
    const QString id = m_selectedObjectId;
    commit(
        QStringLiteral("Delete object"),
        [id, this](Presentation &presentation) {
            Slide &slide = presentation.slides[m_currentIndex];
            const int index = slide.indexOfObject(id);
            if (index >= 0)
                slide.objects.removeAt(index);
        });
    m_selectedObjectId.clear();
    emit selectionChanged(QString());
}

void Editor::duplicateSelectedObject()
{
    if (!selectedObject() || !currentSlide())
        return;
    SlideObject copy = *selectedObject();
    copy.id = newId();
    copy.name = QStringLiteral("%1 copy").arg(copy.name);
    copy.frame.translate(24, 24);
    copy.hasFrame = true;
    const QString id = copy.id;
    commit(
        QStringLiteral("Duplicate object"),
        [&copy, this](Presentation &presentation) {
            presentation.slides[m_currentIndex].objects.append(copy);
        });
    m_selectedObjectId = id;
    emit selectionChanged(m_selectedObjectId);
}

void Editor::bringSelectedForward()
{
    if (m_selectedObjectId.isEmpty() || !currentSlide())
        return;
    const QString id = m_selectedObjectId;
    commit(
        QStringLiteral("Bring forward"),
        [id, this](Presentation &presentation) {
            Slide &slide = presentation.slides[m_currentIndex];
            const int index = slide.indexOfObject(id);
            if (index >= 0 && index < slide.objects.size() - 1)
                slide.objects.swapItemsAt(index, index + 1);
        });
}

void Editor::sendSelectedBackward()
{
    if (m_selectedObjectId.isEmpty() || !currentSlide())
        return;
    const QString id = m_selectedObjectId;
    commit(
        QStringLiteral("Send backward"),
        [id, this](Presentation &presentation) {
            Slide &slide = presentation.slides[m_currentIndex];
            const int index = slide.indexOfObject(id);
            if (index > 0)
                slide.objects.swapItemsAt(index, index - 1);
        });
}

void Editor::commit(const QString &label, const std::function<void(Presentation &)> &change)
{
    commit(label, change, false);
}

void Editor::commit(const QString &label, const std::function<void(Presentation &)> &change,
                    bool structural)
{
    if (!m_hasPresentation)
        return;
    const QJsonObject before = m_presentation.toJson();
    change(m_presentation);
    const QJsonObject after = m_presentation.toJson();
    if (before == after)
        return;
    pushUndo(label, before, after);
    afterEdit(structural);
}

void Editor::beginInteractiveEdit()
{
    if (!m_hasPresentation || m_interactive)
        return;
    m_interactive = true;
    m_interactiveBefore = m_presentation.toJson();
}

void Editor::endInteractiveEdit(const QString &label)
{
    if (!m_interactive)
        return;
    m_interactive = false;
    const QJsonObject after = m_presentation.toJson();
    if (after == m_interactiveBefore) {
        m_interactiveBefore = {};
        return;
    }
    pushUndo(label, m_interactiveBefore, after);
    m_interactiveBefore = {};
    afterEdit(false);
}

void Editor::cancelInteractiveEdit()
{
    if (!m_interactive)
        return;
    m_interactive = false;
    const QJsonObject snapshot = m_interactiveBefore;
    m_interactiveBefore = {};
    restoreSnapshot(snapshot);
}

void Editor::notifyEdited()
{
    m_revision++;
    emit presentationEdited();
}

void Editor::afterEdit(bool structural)
{
    if (m_presentation.slides.isEmpty())
        m_presentation.slides.append(makeBlankSlide(1));
    m_currentIndex = std::clamp(m_currentIndex, 0, slideCount() - 1);
    if (m_liveIndex >= slideCount())
        m_liveIndex = slideCount() - 1;
    pruneSelection();
    m_dirty = true;
    m_revision++;
    if (structural)
        emit presentationChanged();
    else
        emit presentationEdited();
    emit selectionChanged(m_selectedObjectId);
    emit dirtyChanged(true);
    emit undoStateChanged();
    emit liveChanged();
    scheduleAutosave();
}

void Editor::pushUndo(const QString &label, const QJsonObject &before, const QJsonObject &after)
{
    m_undo.append(UndoEntry{label, before, after});
    while (m_undo.size() > 100)
        m_undo.removeFirst();
    m_redo.clear();
    emit undoStateChanged();
}

void Editor::restoreSnapshot(const QJsonObject &snapshot)
{
    const int previousIndex = m_currentIndex;
    const int previousLive = m_liveIndex;
    m_presentation = Presentation::fromJson(snapshot);
    m_currentIndex = std::clamp(previousIndex, 0, std::max(0, slideCount() - 1));
    m_liveIndex = previousLive;
    if (m_liveIndex >= slideCount())
        m_liveIndex = slideCount() - 1;
    pruneSelection();
    m_dirty = true;
    m_revision++;
    emit presentationChanged();
    if (m_currentIndex != previousIndex)
        emit slideIndexChanged(m_currentIndex);
    emit selectionChanged(m_selectedObjectId);
    emit dirtyChanged(true);
    emit liveChanged();
    emit undoStateChanged();
    scheduleAutosave();
}

void Editor::ensureThemeAvailable()
{
    if (m_themes.isEmpty()) {
        Theme fallback = defaultTheme();
        QString error;
        m_library->saveTheme(fallback, &error);
        m_themes.append(fallback);
    }
    if (m_presentation.themeId.isEmpty()
        || std::none_of(m_themes.begin(), m_themes.end(), [this](const Theme &candidate) {
               return candidate.id == m_presentation.themeId;
           })) {
        m_presentation.themeId = m_themes.first().id;
    }
    m_theme = theme();
}

QString Editor::uniqueSlideName() const
{
    QSet<QString> names;
    for (const Slide &slide : m_presentation.slides)
        names.insert(slide.name.toCaseFolded());
    int number = slideCount() + 1;
    QString candidate = QStringLiteral("Slide %1").arg(number);
    while (names.contains(candidate.toCaseFolded()))
        candidate = QStringLiteral("Slide %1").arg(++number);
    return candidate;
}

void Editor::pruneSelection()
{
    const Slide *slide = currentSlide();
    if (!m_selectedObjectId.isEmpty() && (!slide || slide->indexOfObject(m_selectedObjectId) < 0))
        m_selectedObjectId.clear();
}

void Editor::undo()
{
    if (m_undo.isEmpty())
        return;
    const UndoEntry entry = m_undo.takeLast();
    m_redo.append(entry);
    restoreSnapshot(entry.before);
    emit statusMessage(QStringLiteral("Undo: %1").arg(entry.label), 1600);
}

void Editor::redo()
{
    if (m_redo.isEmpty())
        return;
    const UndoEntry entry = m_redo.takeLast();
    m_undo.append(entry);
    restoreSnapshot(entry.after);
    emit statusMessage(QStringLiteral("Redo: %1").arg(entry.label), 1600);
}

void Editor::setMode(Mode mode)
{
    if (m_mode == mode)
        return;
    if (mode == Mode::Present)
        startPresenting();
    else
        stopPresenting();
}

void Editor::startPresenting()
{
    if (!m_hasPresentation || m_presentation.slides.isEmpty())
        return;
    m_mode = Mode::Present;
    m_liveIndex = m_currentIndex;
    m_blackout = false;
    m_logo = false;
    emit modeChanged();
    emit liveChanged();
    emit statusMessage(QStringLiteral("Presenting “%1”").arg(m_presentation.name), 2200);
}

void Editor::stopPresenting()
{
    if (m_mode == Mode::Edit)
        return;
    m_mode = Mode::Edit;
    m_liveIndex = -1;
    m_blackout = false;
    m_logo = false;
    emit modeChanged();
    emit liveChanged();
}

void Editor::goLive(int index)
{
    if (!m_hasPresentation || m_presentation.slides.isEmpty())
        return;
    index = std::clamp(index, 0, slideCount() - 1);
    m_liveIndex = index;
    m_currentIndex = index;
    m_selectedObjectId.clear();
    emit selectionChanged(QString());
    emit slideIndexChanged(m_currentIndex);
    emit liveChanged();
}

void Editor::next()
{
    if (!m_hasPresentation)
        return;
    if (m_mode != Mode::Present)
        startPresenting();
    if (m_presentation.slides.isEmpty())
        return;
    const int target = std::min((m_liveIndex < 0 ? m_currentIndex : m_liveIndex) + 1,
                                slideCount() - 1);
    goLive(target);
}

void Editor::previous()
{
    if (!m_hasPresentation)
        return;
    if (m_mode != Mode::Present)
        startPresenting();
    if (m_presentation.slides.isEmpty())
        return;
    const int target = std::max((m_liveIndex < 0 ? m_currentIndex : m_liveIndex) - 1, 0);
    goLive(target);
}

void Editor::goToSlideNumber(int number)
{
    if (!m_hasPresentation)
        return;
    const int index = std::clamp(number - 1, 0, slideCount() - 1);
    if (m_mode != Mode::Present)
        startPresenting();
    goLive(index);
}

void Editor::setBlackout(bool blackout)
{
    if (m_blackout == blackout)
        return;
    m_blackout = blackout;
    if (blackout)
        m_logo = false;
    emit liveChanged();
}

void Editor::setLogoVisible(bool visible)
{
    if (m_logo == visible)
        return;
    m_logo = visible;
    if (visible)
        m_blackout = false;
    emit liveChanged();
}

int Editor::timerRemainingMs() const
{
    if (m_timerRunning)
        return static_cast<int>(std::max<qint64>(0, m_timerDeadlineMs - QDateTime::currentMSecsSinceEpoch()));
    return static_cast<int>(std::max<qint64>(0, m_pausedRemainingMs));
}

int Editor::timerRemainingSeconds() const
{
    const int milliseconds = timerRemainingMs();
    return (milliseconds + 999) / 1000;
}

void Editor::startTimer()
{
    if (m_timerRunning)
        return;
    if (m_pausedRemainingMs <= 0)
        m_pausedRemainingMs = m_timerTotalSeconds * 1000LL;
    m_timerDeadlineMs = QDateTime::currentMSecsSinceEpoch() + m_pausedRemainingMs;
    m_timerRunning = true;
    m_timerTick->start();
    emit timerChanged();
}

void Editor::pauseTimer()
{
    if (!m_timerRunning)
        return;
    m_pausedRemainingMs = std::max<qint64>(0, m_timerDeadlineMs - QDateTime::currentMSecsSinceEpoch());
    m_timerRunning = false;
    m_timerTick->stop();
    emit timerChanged();
}

void Editor::resetTimer()
{
    m_timerRunning = false;
    m_timerTick->stop();
    m_pausedRemainingMs = m_timerTotalSeconds * 1000LL;
    emit timerChanged();
}

void Editor::setTimerTotalSeconds(int seconds)
{
    m_timerTotalSeconds = std::clamp(seconds, 1, 24 * 60 * 60);
    resetTimer();
}

void Editor::reloadThemes()
{
    m_themes = m_library->themes();
    if (m_themes.isEmpty()) {
        Theme fallback = defaultTheme();
        QString error;
        if (m_library->saveTheme(fallback, &error))
            m_themes.append(fallback);
    }
    if (m_hasPresentation)
        m_theme = theme();
}

void Editor::reloadMedia()
{
    m_mediaPaths = m_library->mediaFileMap();
}

} // namespace mxu
