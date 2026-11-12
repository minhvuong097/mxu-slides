#include "core/presentationfile.h"

#include "miniz.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QTemporaryFile>

#include <cstdio>
#ifdef Q_OS_WIN
#include <string>
#endif

namespace mxu {
namespace {

constexpr const char *kManifestName = "manifest.json";
constexpr const char *kPayloadName = "presentation.json";

void setError(QString *error, const QString &message)
{
    if (error)
        *error = message;
}

// Miniz takes a path through the C runtime. Open through the platform's wide
// API on Windows so presentation paths can contain any Unicode characters.
MZ_FILE *openArchiveFile(const QString &path, bool writing)
{
#ifdef Q_OS_WIN
    const std::wstring nativePath = path.toStdWString();
    return _wfopen(nativePath.c_str(), writing ? L"wb" : L"rb");
#else
    return std::fopen(QFile::encodeName(path).constData(), writing ? "wb" : "rb");
#endif
}

bool addBytes(mz_zip_archive *zip, const QString &name, const QByteArray &bytes)
{
    const QByteArray archiveName = name.toUtf8();
    return mz_zip_writer_add_mem(zip, archiveName.constData(), bytes.constData(),
                                 static_cast<size_t>(bytes.size()),
                                 MZ_DEFAULT_COMPRESSION) != MZ_FALSE;
}

QByteArray jsonBytes(const QJsonObject &object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Indented);
}

bool extractEntry(mz_zip_archive *zip, const QString &name, QByteArray *out)
{
    const QByteArray archiveName = name.toUtf8();
    const int index = mz_zip_reader_locate_file(zip, archiveName.constData(), nullptr, 0);
    if (index < 0)
        return false;

    mz_zip_archive_file_stat stat;
    if (mz_zip_reader_file_stat(zip, mz_uint(index), &stat) == MZ_FALSE)
        return false;
    if (stat.m_uncomp_size > static_cast<mz_uint64>(256 * 1024 * 1024))
        return false;

    QByteArray data;
    data.resize(static_cast<qsizetype>(stat.m_uncomp_size));
    if (!data.isEmpty()
        && mz_zip_reader_extract_to_mem(zip, mz_uint(index), data.data(),
                                        static_cast<size_t>(data.size()), 0) == MZ_FALSE) {
        return false;
    }
    *out = data;
    return true;
}

class Archive {
public:
    ~Archive()
    {
        if (m_open)
            mz_zip_reader_end(&m_archive);
        if (m_file)
            std::fclose(m_file);
    }

    bool open(const QString &path)
    {
        m_file = openArchiveFile(path, false);
        if (!m_file)
            return false;
        m_open = mz_zip_reader_init_cfile(&m_archive, m_file, 0, 0) != MZ_FALSE;
        if (!m_open) {
            std::fclose(m_file);
            m_file = nullptr;
        }
        return m_open;
    }

    mz_zip_archive *operator->() { return &m_archive; }
    mz_zip_archive *get() { return &m_archive; }

private:
    mz_zip_archive m_archive{};
    MZ_FILE *m_file = nullptr;
    bool m_open = false;
};

QString extensionFor(const MediaItem &item)
{
    QString suffix = QFileInfo(item.fileName).suffix().toLower();
    if (suffix.isEmpty()) {
        suffix = item.mediaKind == QLatin1String("video") ? QStringLiteral("mp4")
                                                         : QStringLiteral("png");
    }
    return suffix;
}

} // namespace

QString PresentationFile::fileNameFor(const Presentation &presentation)
{
    QString base = presentation.name.trimmed();
    if (base.isEmpty())
        base = QStringLiteral("Untitled");
    static const QRegularExpression invalid(QStringLiteral(R"([<>:"/\\|?*\x00-\x1F])"));
    base.replace(invalid, QStringLiteral("-"));
    base = base.simplified();
    while (base.endsWith(QLatin1Char('.')) || base.endsWith(QLatin1Char(' ')))
        base.chop(1);
    if (base.isEmpty())
        base = QStringLiteral("Untitled");
    return base + QStringLiteral(".mxuslides");
}

bool PresentationFile::write(const Bundle &bundle, const QString &path, QString *error)
{
    if (bundle.presentation.slides.isEmpty()
        && (bundle.presentation.id.isEmpty() || bundle.presentation.name.isEmpty())) {
        setError(error, QStringLiteral("There is no presentation to export."));
        return false;
    }

    const QFileInfo destination(path);
    if (!destination.dir().exists() && !QDir().mkpath(destination.absolutePath())) {
        setError(error, QStringLiteral("Could not create the export folder."));
        return false;
    }

    QTemporaryFile temporary(destination.absolutePath() + QStringLiteral("/.mxu-XXXXXX"));
    temporary.setAutoRemove(false);
    if (!temporary.open()) {
        setError(error, QStringLiteral("Could not create a temporary file for the export."));
        return false;
    }
    const QString stagingPath = temporary.fileName();
    temporary.close();

    MZ_FILE *zipFile = openArchiveFile(stagingPath, true);
    if (!zipFile) {
        QFile::remove(stagingPath);
        setError(error, QStringLiteral("Could not start writing the presentation bundle."));
        return false;
    }
    mz_zip_archive zip{};
    if (mz_zip_writer_init_cfile(&zip, zipFile, 0) == MZ_FALSE) {
        std::fclose(zipFile);
        QFile::remove(stagingPath);
        setError(error, QStringLiteral("Could not start writing the presentation bundle."));
        return false;
    }

    bool ok = true;
    QString detail;

    QJsonObject manifest;
    manifest.insert(QStringLiteral("formatVersion"), FormatVersion);
    manifest.insert(QStringLiteral("createdAt"),
                    QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    manifest.insert(QStringLiteral("name"), bundle.presentation.name);
    manifest.insert(QStringLiteral("slideCount"), bundle.presentation.slides.size());
    if (!addBytes(&zip, QString::fromLatin1(kManifestName), jsonBytes(manifest))) {
        ok = false;
        detail = QStringLiteral("Could not write the bundle manifest.");
    }

    QJsonArray themes;
    for (const Theme &theme : bundle.themes)
        themes.append(theme.toJson());
    QJsonArray media;
    for (const MediaItem &item : bundle.media)
        media.append(item.toJson());
    QJsonObject payload;
    payload.insert(QStringLiteral("presentation"), bundle.presentation.toJson());
    payload.insert(QStringLiteral("themes"), themes);
    payload.insert(QStringLiteral("media"), media);
    payload.insert(QStringLiteral("audio"), QJsonArray());
    if (ok && !addBytes(&zip, QString::fromLatin1(kPayloadName), jsonBytes(payload))) {
        ok = false;
        detail = QStringLiteral("Could not write the presentation payload.");
    }

    if (ok) {
        QSet<QString> written;
        for (const MediaItem &item : bundle.media) {
            if (item.fileHash.isEmpty() || written.contains(item.fileHash))
                continue;
            const auto blob = bundle.blobs.constFind(item.fileHash);
            if (blob == bundle.blobs.constEnd() || blob.value().isEmpty()) {
                // Missing media is legal: the receiving library reports it, just
                // as the macOS exporter does.
                continue;
            }
            const QString name = QStringLiteral("blobs/%1.%2")
                                     .arg(item.fileHash, extensionFor(item));
            if (!addBytes(&zip, name, blob.value())) {
                ok = false;
                detail = QStringLiteral("Could not write media “%1” into the bundle.").arg(item.name);
                break;
            }
            written.insert(item.fileHash);
        }
    }

    if (ok && mz_zip_writer_finalize_archive(&zip) == MZ_FALSE) {
        ok = false;
        detail = QStringLiteral("Could not finish the presentation bundle.");
    }
    if (!mz_zip_writer_end(&zip) && ok) {
        ok = false;
        detail = QStringLiteral("Could not close the presentation bundle.");
    }
    if (std::fclose(zipFile) != 0 && ok) {
        ok = false;
        detail = QStringLiteral("Could not finish writing the presentation bundle.");
    }

    if (!ok) {
        QFile::remove(stagingPath);
        setError(error, detail);
        return false;
    }

    if (QFile::exists(path) && !QFile::remove(path)) {
        QFile::remove(stagingPath);
        setError(error, QStringLiteral("The destination file is in use. Close it and try again."));
        return false;
    }
    if (!QFile::rename(stagingPath, path)) {
        QFile::remove(stagingPath);
        setError(error, QStringLiteral("Could not write “%1”.").arg(destination.fileName()));
        return false;
    }
    return true;
}

bool PresentationFile::read(const QString &path, Bundle *bundle, QString *error)
{
    if (!bundle) {
        setError(error, QStringLiteral("No destination was supplied for the presentation."));
        return false;
    }
    const QFileInfo info(path);
    if (!info.exists() || !info.isFile()) {
        setError(error, QStringLiteral("“%1” does not exist.").arg(info.fileName()));
        return false;
    }

    Archive archive;
    if (!archive.open(path)) {
        setError(error, QStringLiteral("“%1” isn't an MxU Slides presentation.").arg(info.fileName()));
        return false;
    }

    QByteArray manifestBytes;
    if (!extractEntry(archive.get(), QString::fromLatin1(kManifestName), &manifestBytes)) {
        setError(error, QStringLiteral("“%1” isn't an MxU Slides presentation.").arg(info.fileName()));
        return false;
    }
    QJsonParseError parseError{};
    const QJsonDocument manifestDocument = QJsonDocument::fromJson(manifestBytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !manifestDocument.isObject()) {
        setError(error, QStringLiteral("The bundle manifest is damaged."));
        return false;
    }
    const int version = manifestDocument.object().value(QStringLiteral("formatVersion")).toInt();
    if (version > FormatVersion) {
        setError(error,
                 QStringLiteral("This presentation was made by a newer MxU Slides (format %1). "
                                "Update the app to import it.")
                     .arg(version));
        return false;
    }

    QByteArray payloadBytes;
    if (!extractEntry(archive.get(), QString::fromLatin1(kPayloadName), &payloadBytes)) {
        setError(error, QStringLiteral("The presentation payload is missing from the bundle."));
        return false;
    }
    const QJsonDocument payloadDocument = QJsonDocument::fromJson(payloadBytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !payloadDocument.isObject()) {
        setError(error, QStringLiteral("The presentation payload is damaged."));
        return false;
    }
    const QJsonObject payload = payloadDocument.object();
    const QJsonObject presentationObject = payload.value(QStringLiteral("presentation")).toObject();
    if (presentationObject.isEmpty() || !presentationObject.contains(QStringLiteral("slides"))) {
        setError(error, QStringLiteral("The bundle does not contain a presentation."));
        return false;
    }

    Bundle contents;
    contents.presentation = Presentation::fromJson(presentationObject);
    for (const QJsonValue &value : payload.value(QStringLiteral("themes")).toArray())
        contents.themes.append(Theme::fromJson(value.toObject()));
    for (const QJsonValue &value : payload.value(QStringLiteral("media")).toArray())
        contents.media.append(MediaItem::fromJson(value.toObject()));

    const mz_uint fileCount = mz_zip_reader_get_num_files(archive.get());
    for (mz_uint index = 0; index < fileCount; ++index) {
        char nameBuffer[1024] = {};
        if (mz_zip_reader_get_filename(archive.get(), index, nameBuffer, sizeof(nameBuffer)) == 0)
            continue;
        const QString name = QString::fromUtf8(nameBuffer);
        if (!name.startsWith(QStringLiteral("blobs/")) || name.endsWith(QLatin1Char('/')))
            continue;
        const QString baseName = name.section(QLatin1Char('/'), -1);
        const QString hash = baseName.section(QLatin1Char('.'), 0, 0);
        if (hash.isEmpty())
            continue;
        QByteArray bytes;
        if (extractEntry(archive.get(), name, &bytes))
            contents.blobs.insert(hash, bytes);
    }

    if (contents.presentation.name.isEmpty())
        contents.presentation.name = QStringLiteral("Imported presentation");
    *bundle = contents;
    return true;
}

} // namespace mxu
