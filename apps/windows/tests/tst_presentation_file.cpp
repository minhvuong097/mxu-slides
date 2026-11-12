#include "core/presentationfile.h"

#include "miniz.h"

#include <QBuffer>
#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtTest>

using namespace mxu;

namespace {

QByteArray pngBytes()
{
    QImage image(8, 8, QImage::Format_ARGB32);
    image.fill(QColor(32, 96, 180));
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return bytes;
}

Bundle sampleBundle()
{
    Bundle bundle;
    bundle.presentation = makePresentation(QStringLiteral("Sunday Morning"), defaultTheme());
    bundle.presentation.slides[0].objects.append(
        makeTextObject(QStringLiteral("Lyrics"), QStringLiteral("Amazing grace"),
                       QRectF(160, 300, 1600, 400), 96));
    bundle.themes.append(defaultTheme());

    MediaItem image;
    image.id = QStringLiteral("media-1");
    image.name = QStringLiteral("Background");
    image.fileName = QStringLiteral("background.png");
    image.fileHash = QStringLiteral("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");
    bundle.media.append(image);
    bundle.blobs.insert(image.fileHash, pngBytes());
    return bundle;
}

} // namespace

class PresentationFileTest : public QObject {
    Q_OBJECT

private slots:
    void bundleRoundTrips();
    void bundleRoundTripsOnUnicodePath();
    void rejectsNonBundles();
    void rejectsNewerFormats();
    void fileNameIsSafe();
};

void PresentationFileTest::bundleRoundTrips()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("test.mxuslides"));
    QString error;
    QVERIFY2(PresentationFile::write(sampleBundle(), path, &error), qPrintable(error));
    QVERIFY(QFileInfo::exists(path));

    QFile raw(path);
    QVERIFY(raw.open(QIODevice::ReadOnly));
    QVERIFY(raw.read(2) == QByteArray("PK"));

    Bundle restored;
    QVERIFY2(PresentationFile::read(path, &restored, &error), qPrintable(error));
    QCOMPARE(restored.presentation.name, QStringLiteral("Sunday Morning"));
    QCOMPARE(restored.presentation.slides.size(), 1);
    QCOMPARE(restored.presentation.slides[0].objects.size(), 1);
    QCOMPARE(restored.themes.size(), 1);
    QCOMPARE(restored.media.size(), 1);
    QCOMPARE(restored.blobs.size(), 1);
    QVERIFY(restored.blobs.value(restored.media[0].fileHash) == pngBytes());
}

void PresentationFileTest::bundleRoundTripsOnUnicodePath()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("Chúa nhật · 日本語 · 🎤.mxuslides"));
    QString error;
    QVERIFY2(PresentationFile::write(sampleBundle(), path, &error), qPrintable(error));
    QVERIFY(QFileInfo::exists(path));

    Bundle restored;
    QVERIFY2(PresentationFile::read(path, &restored, &error), qPrintable(error));
    QCOMPARE(restored.presentation.name, QStringLiteral("Sunday Morning"));
    QCOMPARE(restored.blobs.size(), 1);
}

void PresentationFileTest::rejectsNonBundles()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("notes.mxuslides"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("this is not a zip");
    file.close();

    Bundle bundle;
    QString error;
    QVERIFY(!PresentationFile::read(path, &bundle, &error));
    QVERIFY(error.contains(QStringLiteral("isn't an MxU Slides presentation")));
}

void PresentationFileTest::rejectsNewerFormats()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("newer.mxuslides"));

    mz_zip_archive zip{};
    QVERIFY(mz_zip_writer_init_file(&zip, QFile::encodeName(path).constData(), 0) != MZ_FALSE);
    const QByteArray manifest = R"({"formatVersion":99,"createdAt":"2026-11-10T00:00:00Z","name":"x","slideCount":0})";
    QVERIFY(mz_zip_writer_add_mem(&zip, "manifest.json", manifest.constData(),
                                  size_t(manifest.size()), MZ_DEFAULT_COMPRESSION) != MZ_FALSE);
    const QByteArray payload = R"({"presentation":{"id":"x","name":"x","presentationKind":"deck","themeId":"t","slides":[]},"themes":[],"media":[],"audio":[]})";
    QVERIFY(mz_zip_writer_add_mem(&zip, "presentation.json", payload.constData(),
                                  size_t(payload.size()), MZ_DEFAULT_COMPRESSION) != MZ_FALSE);
    QVERIFY(mz_zip_writer_finalize_archive(&zip) != MZ_FALSE);
    mz_zip_writer_end(&zip);

    Bundle bundle;
    QString error;
    QVERIFY(!PresentationFile::read(path, &bundle, &error));
    QVERIFY(error.contains(QStringLiteral("newer MxU Slides")));
}

void PresentationFileTest::fileNameIsSafe()
{
    Presentation presentation = makePresentation(QStringLiteral("Sunday: 9/14"), defaultTheme());
    QCOMPARE(PresentationFile::fileNameFor(presentation), QStringLiteral("Sunday- 9-14.mxuslides"));
    presentation.name = QStringLiteral("   ");
    QCOMPARE(PresentationFile::fileNameFor(presentation), QStringLiteral("Untitled.mxuslides"));
}

QTEST_MAIN(PresentationFileTest)
#include "tst_presentation_file.moc"
