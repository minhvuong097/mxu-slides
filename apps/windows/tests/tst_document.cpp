#include "core/document.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

using namespace mxu;

class DocumentTest : public QObject {
    Q_OBJECT

private slots:
    void idsAreUnique();
    void colorsRoundTrip();
    void presentationRoundTripsAndPreservesUnknownFields();
    void optionalGeometryIsOptional();
    void textLinkSurvivesRoundTrip();
    void factoriesProduceEditableSlides();
};

void DocumentTest::idsAreUnique()
{
    QVERIFY(newId() != newId());
    QVERIFY(!newId().isEmpty());
}

void DocumentTest::colorsRoundTrip()
{
    QCOMPARE(colorToHex(QColor(255, 128, 0)), QStringLiteral("#FF8000"));
    QCOMPARE(colorToHex(QColor(10, 20, 30, 128)), QStringLiteral("#0A141E80"));
    QCOMPARE(colorFromHex(QStringLiteral("#FF8000")), QColor(255, 128, 0));
    QCOMPARE(colorFromHex(QStringLiteral("0A141E80")), QColor(10, 20, 30, 128));
    QCOMPARE(colorFromHex(QString(), QColor(Qt::red)), QColor(Qt::red));
}

void DocumentTest::presentationRoundTripsAndPreservesUnknownFields()
{
    Presentation deck = makePresentation(QStringLiteral("Sunday"), defaultTheme());
    SlideObject text = makeTextObject(QStringLiteral("Lyrics"),
                                      QStringLiteral("Amazing grace\nHow sweet the sound"),
                                      QRectF(160, 300, 1600, 480), 92);
    text.textStyle.colorSet = true;
    text.textStyle.color = QColor(255, 240, 210);
    text.extra.insert(QStringLiteral("animationSteps"), QJsonArray{QJsonObject{{"id", "a1"}}});
    deck.slides[0].objects.append(text);
    deck.slides[0].objects.append(makeShapeObject(QStringLiteral("Bar"), QStringLiteral("roundedRectangle"),
                                                   QRectF(80, 860, 700, 90), QColor(30, 60, 110, 210)));
    deck.slides[0].backgroundFill = Fill::solid(QColor(12, 20, 34));
    deck.transition.enabled = true;
    deck.transition.kind = QStringLiteral("dissolve");
    deck.transition.durationSeconds = 0.6;

    const QJsonObject encoded = deck.toJson();
    QCOMPARE(encoded.value(QStringLiteral("presentationKind")).toString(), QStringLiteral("deck"));
    QCOMPARE(encoded.value(QStringLiteral("slides")).toArray().size(), 1);

    Presentation decoded = Presentation::fromJson(encoded);
    QCOMPARE(decoded.id, deck.id);
    QCOMPARE(decoded.name, deck.name);
    QCOMPARE(decoded.themeId, deck.themeId);
    QCOMPARE(decoded.slides.size(), 1);
    QCOMPARE(decoded.slides[0].objects.size(), 2);
    QCOMPARE(decoded.slides[0].objects[0].text, text.text);
    QCOMPARE(decoded.slides[0].objects[0].textStyle.fontSize, 92.0);
    QCOMPARE(decoded.slides[0].objects[0].textStyle.color, QColor(255, 240, 210));
    QCOMPARE(decoded.slides[0].objects[0].frame, QRectF(160, 300, 1600, 480));
    QCOMPARE(decoded.slides[0].backgroundFill.color, QColor(12, 20, 34));
    QCOMPARE(decoded.transition.durationSeconds, 0.6);
    QVERIFY(decoded.slides[0].objects[0].extra.contains(QStringLiteral("animationSteps")));
    QCOMPARE(decoded.toJson().value(QStringLiteral("slides"))
                 .toArray()[0].toObject()
                 .value(QStringLiteral("objects")).toArray()[0].toObject()
                 .value(QStringLiteral("animationSteps")).toArray().size(), 1);
}

void DocumentTest::optionalGeometryIsOptional()
{
    const QJsonObject raw{
        {"id", "o1"},
        {"objectKind", "text"},
        {"name", "Lyrics"},
        {"text", "Hello"},
        {"textStyle", QJsonObject{{"fontName", "Segoe UI"}, {"fontSize", 88.0}}},
    };
    const SlideObject object = SlideObject::fromJson(raw);
    QVERIFY(!object.hasFrame);
    QCOMPARE(object.frame.width(), 1680.0);
    const QJsonObject encoded = object.toJson();
    QVERIFY(!encoded.contains(QStringLiteral("x")));
    QVERIFY(!encoded.contains(QStringLiteral("width")));
    QCOMPARE(encoded.value(QStringLiteral("textStyle")).toObject()
                 .value(QStringLiteral("fontName")).toString(), QStringLiteral("Segoe UI"));
}

void DocumentTest::textLinkSurvivesRoundTrip()
{
    SlideObject clock = makeTextObject(QStringLiteral("Clock"), QString(), QRectF(0, 0, 400, 100));
    clock.textLink = QJsonObject{{"source", "clock"}, {"clockFormat", "h:mm"}};
    const SlideObject restored = SlideObject::fromJson(clock.toJson());
    QCOMPARE(restored.textLink.value(QStringLiteral("source")).toString(), QStringLiteral("clock"));
}

void DocumentTest::factoriesProduceEditableSlides()
{
    const Theme theme = defaultTheme();
    const Presentation deck = makePresentation(QStringLiteral("New Deck"), theme);
    QCOMPARE(deck.slides.size(), 1);
    QCOMPARE(deck.themeId, theme.id);
    QCOMPARE(deck.canvasWidth, 1920);
    QCOMPARE(deck.canvasHeight, 1080);

    const SlideObject shape = makeShapeObject(QStringLiteral("Lower third"),
                                              QStringLiteral("roundedRectangle"),
                                              QRectF(80, 800, 800, 120), QColor(20, 40, 70));
    QVERIFY(shape.isShape());
    QVERIFY(shape.fill.visible());
    QCOMPARE(shape.fill.kind, QStringLiteral("solid"));
    QVERIFY(!shape.id.isEmpty());

    const SlideObject text = makeTextObject(QStringLiteral("Title"), QStringLiteral("Hello"),
                                            QRectF(100, 100, 800, 200), 72);
    QVERIFY(text.isText());
    QCOMPARE(slidePlainText(Slide{QStringLiteral("s"), QStringLiteral("Slide"), {text}, {}, {}, {}, {}}),
             QStringLiteral("Hello"));
}

QTEST_MAIN(DocumentTest)
#include "tst_document.moc"
