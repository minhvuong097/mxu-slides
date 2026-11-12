#include "core/library.h"
#include "core/presentationfile.h"

#include <QBuffer>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using namespace mxu;

class LibraryTest : public QObject {
    Q_OBJECT

private slots:
    void savesLoadsDuplicatesAndDeletes();
    void importsMediaIntoBlobStore();
    void seedStarterContentIsIdempotent();
    void exportedBundleImportsIntoAnotherLibrary();
    void uniqueNamesAvoidCollisions();
};

void LibraryTest::savesLoadsDuplicatesAndDeletes()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    Library library(directory.filePath(QStringLiteral("Library")));
    QString error;
    QVERIFY2(library.ensureLayout(&error), qPrintable(error));

    Presentation deck = makePresentation(QStringLiteral("Sunday"), defaultTheme());
    QVERIFY2(library.savePresentation(deck, &error), qPrintable(error));
    QCOMPARE(library.entries().size(), 1);
    QCOMPARE(library.entries().first().name, QStringLiteral("Sunday"));
    QCOMPARE(library.entries().first().slideCount, 1);

    Presentation loaded;
    QVERIFY2(library.loadPresentation(deck.id, &loaded, &error), qPrintable(error));
    QCOMPARE(loaded.name, deck.name);
    QCOMPARE(loaded.slides.size(), 1);

    const QString copyId = library.duplicatePresentation(deck.id, &error);
    QVERIFY2(!copyId.isEmpty(), qPrintable(error));
    QCOMPARE(library.entries().size(), 2);
    Presentation copy;
    QVERIFY(library.loadPresentation(copyId, &copy));
    QVERIFY(copy.id != deck.id);
    QVERIFY(copy.name != deck.name);

    QVERIFY(library.deletePresentation(copyId, &error));
    QCOMPARE(library.entries().size(), 1);
    QVERIFY(!library.loadPresentation(copyId, &copy, &error));
}

void LibraryTest::importsMediaIntoBlobStore()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    Library library(directory.filePath(QStringLiteral("Library")));
    QVERIFY(library.ensureLayout());

    QImage image(4, 4, QImage::Format_ARGB32);
    image.fill(QColor(200, 30, 40));
    const QString source = directory.filePath(QStringLiteral("background.png"));
    QVERIFY(image.save(source));

    MediaItem item;
    QString error;
    const QString id = library.importMedia(source, QStringLiteral("Red background"), &item, &error);
    QVERIFY2(!id.isEmpty(), qPrintable(error));
    QCOMPARE(item.fileHash.size(), 64);
    QVERIFY(QFileInfo::exists(library.blobPath(item.fileHash)));
    QCOMPARE(library.mediaItems().size(), 1);

    const QHash<QString, QString> map = library.mediaFileMap();
    QVERIFY(map.contains(id));
    QVERIFY(QFileInfo::exists(map.value(id)));
}

void LibraryTest::seedStarterContentIsIdempotent()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    Library library(directory.filePath(QStringLiteral("Library")));
    QString error;
    QVERIFY2(library.seedStarterContent(&error), qPrintable(error));
    const int count = library.entries().size();
    QCOMPARE(count, 1);
    QVERIFY(!library.themes().isEmpty());
    QVERIFY2(library.seedStarterContent(&error), qPrintable(error));
    QCOMPARE(library.entries().size(), count);
}

void LibraryTest::exportedBundleImportsIntoAnotherLibrary()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    Library source(directory.filePath(QStringLiteral("Source")));
    QVERIFY(source.seedStarterContent());

    const LibraryEntry entry = source.entries().first();
    Presentation deck;
    QVERIFY(source.loadPresentation(entry.id, &deck));

    QImage image(16, 16, QImage::Format_ARGB32);
    image.fill(QColor(8, 90, 160));
    const QString imagePath = directory.filePath(QStringLiteral("bg.png"));
    QVERIFY(image.save(imagePath));
    MediaItem media;
    QString error;
    const QString mediaId = source.importMedia(imagePath, QStringLiteral("Blue"), &media, &error);
    QVERIFY2(!mediaId.isEmpty(), qPrintable(error));

    SlideObject picture;
    picture.id = newId();
    picture.objectKind = QStringLiteral("media");
    picture.name = QStringLiteral("Background");
    picture.hasFrame = true;
    picture.frame = QRectF(0, 0, 1920, 1080);
    picture.mediaId = media.id;
    deck.slides[0].objects.append(picture);
    QVERIFY(source.savePresentation(deck, &error));

    QStringList warnings;
    const Bundle bundle = source.bundleFor(deck, &warnings);
    QVERIFY(warnings.isEmpty());
    QCOMPARE(bundle.media.size(), 1);
    QCOMPARE(bundle.blobs.size(), 1);

    const QString bundlePath = directory.filePath(QStringLiteral("deck.mxuslides"));
    QVERIFY2(PresentationFile::write(bundle, bundlePath, &error), qPrintable(error));

    Library target(directory.filePath(QStringLiteral("Target")));
    QString importedId;
    QStringList importWarnings;
    QVERIFY2(target.importBundle(bundlePath, &importedId, &error, &importWarnings),
             qPrintable(error));
    QVERIFY2(importWarnings.isEmpty(), qPrintable(importWarnings.join(QStringLiteral(", "))));

    Presentation imported;
    QVERIFY(target.loadPresentation(importedId, &imported));
    QCOMPARE(imported.slides.size(), deck.slides.size());
    const QString importedMediaId = imported.slides[0].objects.last().mediaId;
    QVERIFY(!importedMediaId.isEmpty());
    QVERIFY(target.mediaFileMap().contains(importedMediaId));
    QVERIFY(target.themes().size() >= 1);
}

void LibraryTest::uniqueNamesAvoidCollisions()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    Library library(directory.filePath(QStringLiteral("Library")));
    QString error;
    Presentation a = makePresentation(QStringLiteral("Sunday"), defaultTheme());
    QVERIFY(library.savePresentation(a, &error));
    Presentation b = makePresentation(QStringLiteral("Sunday"), defaultTheme());
    QVERIFY(library.savePresentation(b, &error));
    QCOMPARE(library.uniquePresentationName(QStringLiteral("Sunday")), QStringLiteral("Sunday 2"));
    QCOMPARE(library.uniquePresentationName(QStringLiteral("Saturday")), QStringLiteral("Saturday"));
}

QTEST_MAIN(LibraryTest)
#include "tst_library.moc"
