#include "core/document.h"
#include "render/sliderenderer.h"

#include <QtTest>

using namespace mxu;

namespace {

Presentation deckWith(const SlideObject &object)
{
    Presentation deck = makePresentation(QStringLiteral("Test"), defaultTheme());
    deck.slides[0].objects.append(object);
    return deck;
}

} // namespace

class RendererTest : public QObject {
    Q_OBJECT

private slots:
    void paintsBackgroundAndObjects();
    void textRendersVisiblePixels();
    void gradientFillsProduceVariation();
    void dynamicClockChangesImage();
    void missingMediaDrawsPlaceholder();
};

void RendererTest::paintsBackgroundAndObjects()
{
    const Theme theme = defaultTheme();
    Presentation deck = makePresentation(QStringLiteral("Test"), theme);
    deck.slides[0].backgroundFill = Fill::solid(QColor(12, 34, 56));
    SlideRenderer renderer;
    const QImage image = renderer.render(deck, deck.slides[0], theme, QSize(640, 360));
    QCOMPARE(image.size(), QSize(640, 360));
    QCOMPARE(image.pixelColor(2, 2), QColor(12, 34, 56));
    QCOMPARE(image.pixelColor(638, 358), QColor(12, 34, 56));
}

void RendererTest::textRendersVisiblePixels()
{
    const Theme theme = defaultTheme();
    SlideObject text = makeTextObject(QStringLiteral("Title"), QStringLiteral("Amazing grace"),
                                      QRectF(160, 300, 1600, 400), 140);
    text.textStyle.colorSet = true;
    text.textStyle.color = Qt::white;
    Presentation deck = deckWith(text);
    deck.slides[0].backgroundFill = Fill::solid(Qt::black);

    SlideRenderer renderer;
    const QImage image = renderer.render(deck, deck.slides[0], theme, QSize(640, 360));
    int brightPixels = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (image.pixelColor(x, y).lightness() > 180)
                ++brightPixels;
        }
    }
    QVERIFY(brightPixels > 200);
}

void RendererTest::gradientFillsProduceVariation()
{
    const Theme theme = defaultTheme();
    Presentation deck = makePresentation(QStringLiteral("Test"), theme);
    Fill gradient;
    gradient.kind = QStringLiteral("linearGradient");
    gradient.gradientAngle = 0.0;
    gradient.stops = {GradientStop{QColor(255, 0, 0), 0.0},
                      GradientStop{QColor(0, 0, 255), 1.0}};
    deck.slides[0].backgroundFill = gradient;

    SlideRenderer renderer;
    const QImage image = renderer.render(deck, deck.slides[0], theme, QSize(200, 100));
    QVERIFY(image.pixelColor(5, 50) != image.pixelColor(195, 50));
    // The gradient is interpolated, so near-edge pixels are close to (not exactly)
    // the endpoint colors.
    QVERIFY(image.pixelColor(2, 50).red() > 220);
    QVERIFY(image.pixelColor(2, 50).blue() < 40);
    QVERIFY(image.pixelColor(198, 50).blue() > 220);
    QVERIFY(image.pixelColor(198, 50).red() < 40);
}

void RendererTest::dynamicClockChangesImage()
{
    const Theme theme = defaultTheme();
    SlideObject clock = makeTextObject(QStringLiteral("Clock"), QString(), QRectF(200, 300, 1200, 300), 160);
    clock.textLink = QJsonObject{{"source", "clock"}, {"clockFormat", "h:mm AP"}};
    clock.textStyle.colorSet = true;
    clock.textStyle.color = Qt::white;
    Presentation deck = deckWith(clock);
    deck.slides[0].backgroundFill = Fill::solid(Qt::black);

    RenderContext morning;
    morning.clock = QTime(9, 5);
    RenderContext evening;
    evening.clock = QTime(21, 45);
    SlideRenderer renderer;
    const QImage first = renderer.render(deck, deck.slides[0], theme, QSize(480, 270), morning);
    const QImage second = renderer.render(deck, deck.slides[0], theme, QSize(480, 270), evening);
    QVERIFY(first != second);
}

void RendererTest::missingMediaDrawsPlaceholder()
{
    const Theme theme = defaultTheme();
    SlideObject media;
    media.id = newId();
    media.objectKind = QStringLiteral("media");
    media.name = QStringLiteral("Logo");
    media.hasFrame = true;
    media.frame = QRectF(200, 200, 800, 500);
    media.mediaId = QStringLiteral("does-not-exist");
    Presentation deck = deckWith(media);
    deck.slides[0].backgroundFill = Fill::solid(Qt::black);

    SlideRenderer renderer;
    renderer.setMediaResolver([](const QString &) { return QString(); });
    const QImage image = renderer.render(deck, deck.slides[0], theme, QSize(640, 360));
    QVERIFY(image.pixelColor(200, 180) == QColor(20, 25, 34, 220)
            || image.pixelColor(200, 180) != QColor(Qt::black));
}

QTEST_MAIN(RendererTest)
#include "tst_renderer.moc"
