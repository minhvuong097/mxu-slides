#pragma once

#include "core/document.h"

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVector>

#include <functional>

namespace mxu {

class Library;

class Editor : public QObject {
    Q_OBJECT

public:
    enum class Mode { Edit, Present };

    explicit Editor(Library *library, QObject *parent = nullptr);

    Library *library() const { return m_library; }

    bool hasPresentation() const { return m_hasPresentation; }
    const Presentation &presentation() const { return m_presentation; }
    Presentation &mutablePresentation() { return m_presentation; }
    const Slide *currentSlide() const;
    Slide *currentSlideMutable();
    int currentIndex() const { return m_currentIndex; }
    const SlideObject *selectedObject() const;
    QString selectedObjectId() const { return m_selectedObjectId; }
    QString presentationId() const { return m_hasPresentation ? m_presentation.id : QString(); }
    Mode mode() const { return m_mode; }
    bool isPresenting() const { return m_mode == Mode::Present; }
    bool isLive() const { return m_liveIndex >= 0; }
    int liveIndex() const { return m_liveIndex; }
    bool blackout() const { return m_blackout; }
    bool logoVisible() const { return m_logo; }
    bool dirty() const { return m_dirty; }
    int revision() const { return m_revision; }
    Theme theme() const;
    QVector<Theme> themes() const { return m_themes; }
    QString mediaPath(const QString &mediaId) const;
    QHash<QString, QString> mediaPaths() const { return m_mediaPaths; }

    bool open(const QString &presentationId, QString *error = nullptr);
    void createPresentation(const QString &name = {});
    void close();
    bool save(QString *error = nullptr);
    void flushAutosave();
    bool exportBundle(const QString &path, QString *error = nullptr);
    bool importBundle(const QString &path, QString *error = nullptr);
    QString importMediaFile(const QString &sourcePath, QString *error = nullptr,
                            QString *warning = nullptr);

    void setCurrentIndex(int index);
    void addSlide();
    void duplicateSlide();
    void deleteCurrentSlide();
    void moveSlide(int from, int to);

    void selectObject(const QString &objectId);
    void addObject(const SlideObject &object);
    void deleteSelectedObject();
    void duplicateSelectedObject();
    void bringSelectedForward();
    void sendSelectedBackward();

    void commit(const QString &label, const std::function<void(Presentation &)> &change);
    void beginInteractiveEdit();
    void endInteractiveEdit(const QString &label);
    void cancelInteractiveEdit();
    void notifyEdited();

    void undo();
    void redo();

    void setMode(Mode mode);
    void startPresenting();
    void stopPresenting();
    void goLive(int index);
    void next();
    void previous();
    void goToSlideNumber(int number);
    void setBlackout(bool blackout);
    void setLogoVisible(bool visible);

    int timerTotalSeconds() const { return m_timerTotalSeconds; }
    int timerRemainingSeconds() const;
    bool timerRunning() const { return m_timerRunning; }
    void startTimer();
    void pauseTimer();
    void resetTimer();
    void setTimerTotalSeconds(int seconds);

    void reloadThemes();
    void reloadMedia();

    bool canUndo() const { return !m_undo.isEmpty(); }
    bool canRedo() const { return !m_redo.isEmpty(); }
    QString undoLabel() const { return m_undo.isEmpty() ? QString() : m_undo.constLast().label; }
    QString redoLabel() const { return m_redo.isEmpty() ? QString() : m_redo.constLast().label; }

signals:
    void presentationChanged();
    void presentationEdited();
    void slideIndexChanged(int index);
    void selectionChanged(const QString &objectId);
    void liveChanged();
    void modeChanged();
    void dirtyChanged(bool dirty);
    void libraryChanged();
    void timerChanged();
    void undoStateChanged();
    void statusMessage(const QString &message, int timeoutMs);

private:
    struct UndoEntry {
        QString label;
        QJsonObject before;
        QJsonObject after;
    };

    void commit(const QString &label, const std::function<void(Presentation &)> &change,
                bool structural);
    void scheduleAutosave();
    void autosave();
    void afterEdit(bool structural);
    void pushUndo(const QString &label, const QJsonObject &before, const QJsonObject &after);
    void restoreSnapshot(const QJsonObject &snapshot);
    void ensureThemeAvailable();
    QString uniqueSlideName() const;
    void pruneSelection();
    int timerRemainingMs() const;
    int slideCount() const { return static_cast<int>(m_presentation.slides.size()); }

    Library *m_library = nullptr;
    Presentation m_presentation;
    bool m_hasPresentation = false;
    int m_currentIndex = 0;
    int m_liveIndex = -1;
    QString m_selectedObjectId;
    Mode m_mode = Mode::Edit;
    bool m_blackout = false;
    bool m_logo = false;
    bool m_dirty = false;
    int m_revision = 0;

    Theme m_theme;
    QVector<Theme> m_themes;
    QHash<QString, QString> m_mediaPaths;

    QTimer *m_autosave = nullptr;
    bool m_interactive = false;
    QJsonObject m_interactiveBefore;

    QVector<UndoEntry> m_undo;
    QVector<UndoEntry> m_redo;

    QTimer *m_timerTick = nullptr;
    int m_timerTotalSeconds = 300;
    bool m_timerRunning = false;
    qint64 m_timerDeadlineMs = 0;
    qint64 m_pausedRemainingMs = 0;
};

} // namespace mxu
