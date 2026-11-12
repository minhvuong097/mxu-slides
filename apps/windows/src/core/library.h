#pragma once

#include "core/document.h"

#include <QDateTime>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

namespace mxu {

struct LibraryEntry {
    QString id;
    QString name;
    QString folder;
    int slideCount = 0;
    QDateTime updated;
};

// The on-disk library uses the same document layout as the macOS app:
//   <root>/presentations/<id>.json
//   <root>/themes/<id>.json
//   <root>/media/<id>.json
//   <root>/blobs/<sha256>.<ext>
// Documents are JSON rather than Automerge, but .mxuslides bundles move between
// both platforms losslessly for the authoring data modelled here.
class Library {
public:
    explicit Library(QString rootPath = {});

    static QString defaultRootPath();

    QString rootPath() const { return m_rootPath; }
    void setRootPath(const QString &rootPath) { m_rootPath = rootPath; }

    bool ensureLayout(QString *error = nullptr) const;
    QString presentationsDirectory() const;
    QString themesDirectory() const;
    QString mediaDirectory() const;
    QString blobsDirectory() const;
    QString presentationPath(const QString &id) const;

    QVector<LibraryEntry> entries(QString *error = nullptr) const;
    bool loadPresentation(const QString &id, Presentation *out, QString *error = nullptr) const;
    bool savePresentation(const Presentation &presentation, QString *error = nullptr) const;
    bool deletePresentation(const QString &id, QString *error = nullptr) const;
    QString duplicatePresentation(const QString &id, QString *error = nullptr) const;
    QString uniquePresentationName(const QString &base) const;

    QVector<Theme> themes(QString *error = nullptr) const;
    bool saveTheme(const Theme &theme, QString *error = nullptr) const;
    bool loadTheme(const QString &id, Theme *out, QString *error = nullptr) const;

    QVector<MediaItem> mediaItems(QString *error = nullptr) const;
    bool saveMediaItem(const MediaItem &item, QString *error = nullptr) const;
    QString importMedia(const QString &sourceFile, const QString &displayName, MediaItem *out,
                        QString *error = nullptr) const;
    QString blobPath(const QString &hash) const;
    QHash<QString, QString> mediaFileMap(QString *error = nullptr) const;

    Bundle bundleFor(const Presentation &presentation, QStringList *warnings = nullptr) const;
    bool importBundle(const QString &path, QString *newPresentationId, QString *error = nullptr,
                      QStringList *warnings = nullptr) const;

    // First run only: a House theme and a four-slide welcome deck.
    bool seedStarterContent(QString *error = nullptr) const;

private:
    bool writeJson(const QString &path, const QJsonObject &object, QString *error) const;

    QString m_rootPath;
};

} // namespace mxu
