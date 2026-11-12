#include "core/library.h"

#include "core/presentationfile.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>

#include <algorithm>

namespace mxu {
namespace {

QJsonObject readJsonObject(const QString &path, QString *error = nullptr)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("Could not open “%1”.").arg(QFileInfo(path).fileName());
        return {};
    }
    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error)
            *error = QStringLiteral("“%1” is not valid JSON.").arg(QFileInfo(path).fileName());
        return {};
    }
    return document.object();
}

QString sha256ForFile(const QString &path, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("Could not read “%1”.").arg(QFileInfo(path).fileName());
        return {};
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd()) {
        const QByteArray chunk = file.read(4 * 1024 * 1024);
        if (chunk.isEmpty() && file.error() != QFile::NoError) {
            if (error)
                *error = QStringLiteral("Could not finish reading “%1”.").arg(QFileInfo(path).fileName());
            return {};
        }
        hash.addData(chunk);
    }
    return QString::fromLatin1(hash.result().toHex());
}

bool isVideoFile(const QString &path)
{
    static const QSet<QString> extensions{
        QStringLiteral("mp4"), QStringLiteral("mov"), QStringLiteral("m4v"), QStringLiteral("webm"),
        QStringLiteral("avi"), QStringLiteral("mkv"), QStringLiteral("mts"), QStringLiteral("m2ts")};
    return extensions.contains(QFileInfo(path).suffix().toLower());
}

void collectMediaIds(const Slide &slide, QSet<QString> *ids)
{
    if (!slide.backgroundFill.mediaId.isEmpty())
        ids->insert(slide.backgroundFill.mediaId);
    for (const SlideObject &object : slide.objects) {
        if (!object.mediaId.isEmpty())
            ids->insert(object.mediaId);
        if (!object.fill.mediaId.isEmpty())
            ids->insert(object.fill.mediaId);
    }
}

void remapFill(Fill *fill, const QHash<QString, QString> &mediaMap)
{
    const auto found = mediaMap.constFind(fill->mediaId);
    if (found != mediaMap.constEnd())
        fill->mediaId = found.value();
}

QJsonObject starterSlideBackground(const QString &top, const QString &bottom)
{
    Fill fill;
    fill.kind = QStringLiteral("linearGradient");
    fill.gradientAngle = 90.0;
    fill.stops = {GradientStop{colorFromHex(top), 0.0}, GradientStop{colorFromHex(bottom), 1.0}};
    return fill.toJson();
}

} // namespace

Library::Library(QString rootPath)
    : m_rootPath(rootPath.isEmpty() ? defaultRootPath() : std::move(rootPath))
{
}

QString Library::defaultRootPath()
{
    QString documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (documents.isEmpty())
        documents = QDir::homePath() + QStringLiteral("/Documents");
    return documents + QStringLiteral("/MxU Slides");
}

bool Library::ensureLayout(QString *error) const
{
    for (const QString &path : {m_rootPath, presentationsDirectory(), themesDirectory(),
                                mediaDirectory(), blobsDirectory()}) {
        if (!QDir().mkpath(path)) {
            if (error)
                *error = QStringLiteral("Could not create the library folder “%1”.").arg(path);
            return false;
        }
    }
    return true;
}

QString Library::presentationsDirectory() const
{
    return m_rootPath + QStringLiteral("/presentations");
}

QString Library::themesDirectory() const
{
    return m_rootPath + QStringLiteral("/themes");
}

QString Library::mediaDirectory() const
{
    return m_rootPath + QStringLiteral("/media");
}

QString Library::blobsDirectory() const
{
    return m_rootPath + QStringLiteral("/blobs");
}

QString Library::presentationPath(const QString &id) const
{
    return presentationsDirectory() + QLatin1Char('/') + id + QStringLiteral(".json");
}

bool Library::writeJson(const QString &path, const QJsonObject &object, QString *error) const
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = QStringLiteral("Could not write “%1”.").arg(QFileInfo(path).fileName());
        return false;
    }
    file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (error)
            *error = QStringLiteral("Could not finish saving “%1”.").arg(QFileInfo(path).fileName());
        return false;
    }
    return true;
}

QVector<LibraryEntry> Library::entries(QString *error) const
{
    QVector<LibraryEntry> result;
    QDir directory(presentationsDirectory());
    if (!directory.exists()) {
        if (error)
            *error = QStringLiteral("The presentations folder is missing.");
        return result;
    }
    const QFileInfoList files = directory.entryInfoList({QStringLiteral("*.json")}, QDir::Files);
    for (const QFileInfo &file : files) {
        QString parseError;
        const QJsonObject object = readJsonObject(file.absoluteFilePath(), &parseError);
        if (object.isEmpty())
            continue;
        LibraryEntry entry;
        entry.id = object.value(QStringLiteral("id")).toString(file.completeBaseName());
        entry.name = object.value(QStringLiteral("name")).toString(file.completeBaseName());
        entry.folder = object.value(QStringLiteral("folder")).toString();
        entry.slideCount = object.value(QStringLiteral("slides")).toArray().size();
        entry.updated = file.lastModified();
        if (entry.id.isEmpty() || entry.name.isEmpty())
            continue;
        result.append(entry);
    }
    std::sort(result.begin(), result.end(), [](const LibraryEntry &a, const LibraryEntry &b) {
        const int byName = QString::localeAwareCompare(a.name, b.name);
        return byName == 0 ? a.id < b.id : byName < 0;
    });
    return result;
}

bool Library::loadPresentation(const QString &id, Presentation *out, QString *error) const
{
    if (!out) {
        if (error)
            *error = QStringLiteral("No destination was supplied for the presentation.");
        return false;
    }
    const QString path = presentationPath(id);
    if (!QFileInfo::exists(path)) {
        if (error)
            *error = QStringLiteral("That presentation is no longer in the library.");
        return false;
    }
    QString parseError;
    const QJsonObject object = readJsonObject(path, &parseError);
    if (object.isEmpty()) {
        if (error)
            *error = parseError;
        return false;
    }
    Presentation presentation = Presentation::fromJson(object);
    if (presentation.id.isEmpty())
        presentation.id = id;
    if (presentation.id != id)
        presentation.id = id;
    *out = presentation;
    return true;
}

bool Library::savePresentation(const Presentation &presentation, QString *error) const
{
    if (!ensureLayout(error))
        return false;
    if (presentation.id.isEmpty() || presentation.name.isEmpty()) {
        if (error)
            *error = QStringLiteral("A presentation needs an id and a name before it can be saved.");
        return false;
    }
    return writeJson(presentationPath(presentation.id), presentation.toJson(), error);
}

bool Library::deletePresentation(const QString &id, QString *error) const
{
    const QString path = presentationPath(id);
    if (!QFileInfo::exists(path))
        return true;
    if (!QFile::remove(path)) {
        if (error)
            *error = QStringLiteral("Could not delete that presentation. Close it and try again.");
        return false;
    }
    return true;
}

QString Library::duplicatePresentation(const QString &id, QString *error) const
{
    Presentation presentation;
    if (!loadPresentation(id, &presentation, error))
        return {};
    presentation.id = newId();
    presentation.name = uniquePresentationName(presentation.name);
    if (!savePresentation(presentation, error))
        return {};
    return presentation.id;
}

QString Library::uniquePresentationName(const QString &base) const
{
    const QVector<LibraryEntry> existing = entries();
    QSet<QString> names;
    for (const LibraryEntry &entry : existing)
        names.insert(entry.name.toCaseFolded());
    QString candidate = base;
    int suffix = 2;
    while (names.contains(candidate.toCaseFolded()))
        candidate = QStringLiteral("%1 %2").arg(base).arg(suffix++);
    return candidate;
}

QVector<Theme> Library::themes(QString *error) const
{
    QVector<Theme> result;
    QDir directory(themesDirectory());
    if (!directory.exists()) {
        if (error)
            *error = QStringLiteral("The themes folder is missing.");
        return result;
    }
    const QFileInfoList files = directory.entryInfoList({QStringLiteral("*.json")}, QDir::Files);
    for (const QFileInfo &file : files) {
        const QJsonObject object = readJsonObject(file.absoluteFilePath());
        if (object.isEmpty())
            continue;
        Theme theme = Theme::fromJson(object);
        if (theme.id.isEmpty())
            theme.id = file.completeBaseName();
        result.append(theme);
    }
    std::sort(result.begin(), result.end(), [](const Theme &a, const Theme &b) {
        return QString::localeAwareCompare(a.name, b.name) < 0;
    });
    return result;
}

bool Library::saveTheme(const Theme &theme, QString *error) const
{
    if (!ensureLayout(error))
        return false;
    if (theme.id.isEmpty() || theme.name.isEmpty()) {
        if (error)
            *error = QStringLiteral("A theme needs an id and a name.");
        return false;
    }
    const QString path = themesDirectory() + QLatin1Char('/') + theme.id + QStringLiteral(".json");
    return writeJson(path, theme.toJson(), error);
}

bool Library::loadTheme(const QString &id, Theme *out, QString *error) const
{
    if (!out) {
        if (error)
            *error = QStringLiteral("No destination was supplied for the theme.");
        return false;
    }
    const QString path = themesDirectory() + QLatin1Char('/') + id + QStringLiteral(".json");
    if (!QFileInfo::exists(path)) {
        if (error)
            *error = QStringLiteral("That theme is no longer in the library.");
        return false;
    }
    const QJsonObject object = readJsonObject(path, error);
    if (object.isEmpty())
        return false;
    *out = Theme::fromJson(object);
    return true;
}

QVector<MediaItem> Library::mediaItems(QString *error) const
{
    QVector<MediaItem> result;
    QDir directory(mediaDirectory());
    if (!directory.exists()) {
        if (error)
            *error = QStringLiteral("The media folder is missing.");
        return result;
    }
    const QFileInfoList files = directory.entryInfoList({QStringLiteral("*.json")}, QDir::Files);
    for (const QFileInfo &file : files) {
        const QJsonObject object = readJsonObject(file.absoluteFilePath());
        if (object.isEmpty())
            continue;
        MediaItem item = MediaItem::fromJson(object);
        if (item.id.isEmpty())
            continue;
        result.append(item);
    }
    return result;
}

bool Library::saveMediaItem(const MediaItem &item, QString *error) const
{
    if (!ensureLayout(error))
        return false;
    const QString path = mediaDirectory() + QLatin1Char('/') + item.id + QStringLiteral(".json");
    return writeJson(path, item.toJson(), error);
}

QString Library::importMedia(const QString &sourceFile, const QString &displayName,
                             MediaItem *out, QString *error) const
{
    const QFileInfo source(sourceFile);
    if (!source.exists() || !source.isFile()) {
        if (error)
            *error = QStringLiteral("“%1” is not a file.").arg(source.fileName());
        return {};
    }
    if (!ensureLayout(error))
        return {};

    QString hashError;
    const QString hash = sha256ForFile(source.absoluteFilePath(), &hashError);
    if (hash.isEmpty()) {
        if (error)
            *error = hashError;
        return {};
    }

    const QString extension = source.suffix().toLower();
    const QString blobName = extension.isEmpty() ? hash : hash + QLatin1Char('.') + extension;
    const QString destination = blobsDirectory() + QLatin1Char('/') + blobName;
    if (!QFileInfo::exists(destination) && !QFile::copy(source.absoluteFilePath(), destination)) {
        if (error)
            *error = QStringLiteral("Could not copy “%1” into the library.").arg(source.fileName());
        return {};
    }

    MediaItem item;
    item.id = newId();
    item.name = displayName.trimmed().isEmpty() ? source.completeBaseName() : displayName.trimmed();
    item.mediaKind = isVideoFile(source.fileName()) ? QStringLiteral("video") : QStringLiteral("image");
    item.classification = QStringLiteral("background");
    item.fileHash = hash;
    item.fileName = source.fileName();
    item.folder = QStringLiteral("Imported");

    QString saveError;
    if (!saveMediaItem(item, &saveError)) {
        if (error)
            *error = saveError;
        return {};
    }
    if (out)
        *out = item;
    return item.id;
}

QString Library::blobPath(const QString &hash) const
{
    if (hash.isEmpty())
        return {};
    QDir directory(blobsDirectory());
    if (!directory.exists())
        return {};
    const QFileInfoList files = directory.entryInfoList(QDir::Files, QDir::Name);
    for (const QFileInfo &file : files) {
        if (file.completeBaseName().compare(hash, Qt::CaseInsensitive) == 0)
            return file.absoluteFilePath();
        if (file.fileName().compare(hash, Qt::CaseInsensitive) == 0)
            return file.absoluteFilePath();
        if (file.fileName().startsWith(hash, Qt::CaseInsensitive))
            return file.absoluteFilePath();
    }
    return {};
}

QHash<QString, QString> Library::mediaFileMap(QString *error) const
{
    QHash<QString, QString> result;
    for (const MediaItem &item : mediaItems(error)) {
        const QString path = blobPath(item.fileHash);
        if (!path.isEmpty())
            result.insert(item.id, path);
    }
    return result;
}

Bundle Library::bundleFor(const Presentation &presentation, QStringList *warnings) const
{
    Bundle bundle;
    bundle.presentation = presentation;

    QVector<Theme> libraryThemes = themes();
    if (libraryThemes.isEmpty())
        libraryThemes.append(defaultTheme());
    bundle.themes = libraryThemes;

    QSet<QString> referenced;
    if (!presentation.backgroundFill.mediaId.isEmpty())
        referenced.insert(presentation.backgroundFill.mediaId);
    for (const Slide &slide : presentation.slides)
        collectMediaIds(slide, &referenced);

    const QVector<MediaItem> allMedia = mediaItems();
    for (const MediaItem &item : allMedia) {
        if (!referenced.contains(item.id))
            continue;
        bundle.media.append(item);
        const QString path = blobPath(item.fileHash);
        if (path.isEmpty()) {
            if (warnings)
                warnings->append(QStringLiteral("Missing media: %1").arg(item.name));
            continue;
        }
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            if (warnings)
                warnings->append(QStringLiteral("Unreadable media: %1").arg(item.name));
            continue;
        }
        bundle.blobs.insert(item.fileHash, file.readAll());
    }
    return bundle;
}

bool Library::importBundle(const QString &path, QString *newPresentationId, QString *error,
                           QStringList *warnings) const
{
    Bundle bundle;
    if (!PresentationFile::read(path, &bundle, error))
        return false;
    if (!ensureLayout(error))
        return false;

    QVector<Theme> existingThemes = themes();
    QSet<QString> themeIds;
    for (const Theme &theme : existingThemes) {
        themeIds.insert(theme.id);
    }
    for (const Theme &theme : bundle.themes) {
        if (!themeIds.contains(theme.id)) {
            QString themeError;
            if (!saveTheme(theme, &themeError) && warnings)
                warnings->append(themeError);
            themeIds.insert(theme.id);
        }
    }
    if (themeIds.isEmpty()) {
        const Theme fallback = defaultTheme();
        saveTheme(fallback);
        themeIds.insert(fallback.id);
    }

    QVector<MediaItem> existingMedia = mediaItems();
    QHash<QString, QString> existingByHash;
    QSet<QString> mediaIds;
    for (const MediaItem &item : existingMedia) {
        existingByHash.insert(item.fileHash, item.id);
        mediaIds.insert(item.id);
    }

    QHash<QString, QString> mediaMap;
    for (const MediaItem &sourceItem : bundle.media) {
        if (sourceItem.fileHash.isEmpty()) {
            if (warnings)
                warnings->append(QStringLiteral("Media without a file hash: %1").arg(sourceItem.name));
            continue;
        }
        if (existingByHash.contains(sourceItem.fileHash)) {
            mediaMap.insert(sourceItem.id, existingByHash.value(sourceItem.fileHash));
            continue;
        }
        const auto blob = bundle.blobs.constFind(sourceItem.fileHash);
        if (blob == bundle.blobs.constEnd() || blob.value().isEmpty()) {
            if (warnings)
                warnings->append(QStringLiteral("Missing media file: %1").arg(sourceItem.name));
            continue;
        }

        const QString extension = QFileInfo(sourceItem.fileName).suffix().toLower();
        const QString blobName = extension.isEmpty()
            ? sourceItem.fileHash
            : sourceItem.fileHash + QLatin1Char('.') + extension;
        const QString destination = blobsDirectory() + QLatin1Char('/') + blobName;
        if (!QFileInfo::exists(destination)) {
            QFile blobFile(destination);
            if (!blobFile.open(QIODevice::WriteOnly)
                || blobFile.write(blob.value()) != blob.value().size()) {
                if (warnings)
                    warnings->append(QStringLiteral("Could not store media: %1").arg(sourceItem.name));
                continue;
            }
        }

        MediaItem item = sourceItem;
        if (item.id.isEmpty() || mediaIds.contains(item.id))
            item.id = newId();
        QJsonObject folderExtra = item.extra;
        folderExtra.insert(QStringLiteral("folder"), QStringLiteral("Imported"));
        item.extra = folderExtra;
        item.folder = QStringLiteral("Imported");
        QString saveError;
        if (!saveMediaItem(item, &saveError)) {
            if (warnings)
                warnings->append(saveError);
            continue;
        }
        mediaIds.insert(item.id);
        existingByHash.insert(item.fileHash, item.id);
        mediaMap.insert(sourceItem.id, item.id);
    }

    Presentation presentation = bundle.presentation;
    if (presentation.id.isEmpty())
        presentation.id = newId();
    Presentation existing;
    if (loadPresentation(presentation.id, &existing)) {
        presentation.id = newId();
        presentation.name = uniquePresentationName(presentation.name);
    }
    if (presentation.name.trimmed().isEmpty())
        presentation.name = QStringLiteral("Imported presentation");

    if (!themeIds.contains(presentation.themeId)) {
        presentation.themeId = existingThemes.isEmpty() ? defaultTheme().id : existingThemes.first().id;
    }
    for (Slide &slide : presentation.slides) {
        remapFill(&slide.backgroundFill, mediaMap);
        for (SlideObject &object : slide.objects) {
            const auto mapped = mediaMap.constFind(object.mediaId);
            if (mapped != mediaMap.constEnd())
                object.mediaId = mapped.value();
            remapFill(&object.fill, mediaMap);
        }
    }
    remapFill(&presentation.backgroundFill, mediaMap);

    if (!savePresentation(presentation, error))
        return false;
    if (newPresentationId)
        *newPresentationId = presentation.id;
    return true;
}

bool Library::seedStarterContent(QString *error) const
{
    if (!ensureLayout(error))
        return false;
    if (!entries().isEmpty())
        return true;

    Theme theme = defaultTheme();
    theme.id = QStringLiteral("house-theme");
    theme.name = QStringLiteral("House");
    theme.fontFamily = QStringLiteral("Segoe UI");
    theme.fontSize = 96.0;
    theme.textColorHex = QStringLiteral("#F7F5EF");
    theme.backgroundColorHex = QStringLiteral("#0D121A");
    if (!saveTheme(theme, error))
        return false;

    QVector<Theme> libraryThemes = themes();
    const bool themeExists = std::any_of(libraryThemes.begin(), libraryThemes.end(),
                                         [&](const Theme &candidate) {
                                             return candidate.id == theme.id;
                                         });
    if (!themeExists && !saveTheme(theme, error))
        return false;

    Presentation deck;
    deck.id = newId();
    deck.name = QStringLiteral("Welcome to MxU Slides");
    deck.presentationKind = QStringLiteral("deck");
    deck.themeId = theme.id;
    deck.canvasWidth = 1920;
    deck.canvasHeight = 1080;
    deck.folder = QStringLiteral("Getting Started");

    Slide title;
    title.id = newId();
    title.name = QStringLiteral("Welcome");
    title.backgroundFill = Fill::fromJson(
        starterSlideBackground(QStringLiteral("#101A2B"), QStringLiteral("#070A10")));
    SlideObject mark = makeShapeObject(QStringLiteral("Accent bar"), QStringLiteral("roundedRectangle"),
                                       QRectF(160, 272, 96, 12), QColor(90, 141, 238));
    title.objects.append(mark);
    SlideObject heading = makeTextObject(
        QStringLiteral("Title"), QStringLiteral("Welcome to\nMxU Slides"),
        QRectF(160, 320, 1600, 380), 128);
    heading.textStyle.horizontalAlignment = QStringLiteral("left");
    heading.textStyle.verticalAlignment = QStringLiteral("top");
    heading.textStyle.lineHeightMultiple = 1.02;
    title.objects.append(heading);
    SlideObject subtitle = makeTextObject(
        QStringLiteral("Subtitle"), QStringLiteral("A native Windows presentation companion — built with C++ and Qt 6.12."),
        QRectF(160, 730, 1400, 120), 40);
    subtitle.textStyle.horizontalAlignment = QStringLiteral("left");
    subtitle.textStyle.verticalAlignment = QStringLiteral("top");
    subtitle.textStyle.colorSet = true;
    subtitle.textStyle.color = QColor(170, 184, 204);
    title.objects.append(subtitle);
    title.notes = QStringLiteral("Press F5 to take this presentation live. Press Esc to return to the editor.");
    deck.slides.append(title);

    Slide lyrics;
    lyrics.id = newId();
    lyrics.name = QStringLiteral("Lyrics");
    lyrics.backgroundFill = Fill::fromJson(
        starterSlideBackground(QStringLiteral("#182435"), QStringLiteral("#0A0E15")));
    SlideObject panel = makeShapeObject(QStringLiteral("Lower third"),
                                        QStringLiteral("roundedRectangle"),
                                        QRectF(96, 742, 1170, 232), QColor(8, 14, 24, 190));
    panel.cornerRadius = 28;
    lyrics.objects.append(panel);
    SlideObject lyricText = makeTextObject(
        QStringLiteral("Lyrics"),
        QStringLiteral("Amazing grace, how sweet the sound\nThat saved a wretch like me"),
        QRectF(140, 300, 1640, 560), 96);
    lyricText.textStyle.lineHeightMultiple = 1.12;
    lyrics.objects.append(lyricText);
    SlideObject credit = makeTextObject(QStringLiteral("Credit"), QStringLiteral("Traditional · Public domain"),
                                        QRectF(140, 796, 1000, 100), 34);
    credit.textStyle.horizontalAlignment = QStringLiteral("left");
    credit.textStyle.colorSet = true;
    credit.textStyle.color = QColor(180, 190, 204);
    lyrics.objects.append(credit);
    lyrics.notes = QStringLiteral("A lyric slide with its own background fill. Edit text in the inspector on the right.");
    deck.slides.append(lyrics);

    Slide scripture;
    scripture.id = newId();
    scripture.name = QStringLiteral("Scripture");
    scripture.backgroundFill = Fill::fromJson(
        starterSlideBackground(QStringLiteral("#2C2016"), QStringLiteral("#0F0B08")));
    SlideObject verse = makeTextObject(
        QStringLiteral("Verse"),
        QStringLiteral("“Let all that you do\nbe done in love.”"),
        QRectF(220, 250, 1480, 500), 104);
    verse.textStyle.lineHeightMultiple = 1.10;
    scripture.objects.append(verse);
    SlideObject reference = makeTextObject(QStringLiteral("Reference"), QStringLiteral("1 Corinthians 16:14"),
                                           QRectF(220, 790, 1480, 100), 40);
    reference.textStyle.colorSet = true;
    reference.textStyle.color = QColor(238, 185, 95);
    scripture.objects.append(reference);
    scripture.notes = QStringLiteral("Use the slide strip to add, duplicate, and reorder slides.");
    deck.slides.append(scripture);

    Slide closing;
    closing.id = newId();
    closing.name = QStringLiteral("You're ready");
    closing.backgroundFill = Fill::fromJson(
        starterSlideBackground(QStringLiteral("#101A2B"), QStringLiteral("#070A10")));
    SlideObject ready = makeTextObject(QStringLiteral("Heading"), QStringLiteral("You're ready."),
                                       QRectF(180, 300, 1560, 220), 112);
    ready.textStyle.horizontalAlignment = QStringLiteral("left");
    closing.objects.append(ready);
    SlideObject instructions = makeTextObject(
        QStringLiteral("Shortcuts"),
        QStringLiteral("F5   Present\nSpace / →   Next slide\nB   Black the output\nL   Show the logo\nEsc   Stop presenting"),
        QRectF(180, 560, 1100, 340), 42);
    instructions.textStyle.horizontalAlignment = QStringLiteral("left");
    instructions.textStyle.verticalAlignment = QStringLiteral("top");
    instructions.textStyle.lineHeightMultiple = 1.5;
    instructions.textStyle.colorSet = true;
    instructions.textStyle.color = QColor(186, 198, 214);
    closing.objects.append(instructions);
    closing.notes = QStringLiteral("The Program output opens on a second display when one is available.");
    deck.slides.append(closing);

    return savePresentation(deck, error);
}

} // namespace mxu
