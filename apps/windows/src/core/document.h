#pragma once

#include <QColor>
#include <QHash>
#include <QJsonObject>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QVector>

namespace mxu {

// The macOS app stores every document as JSON derived from
// packages/schema/documents.json. These structures cover the presentation
// authoring subset the Windows app can render and edit. Unknown JSON (for
// example animation steps or effects owned by the macOS renderer) is carried in
// each type's "extra" object and written back unchanged, so opening a deck here
// and exporting it again does not silently discard the parts this build does not
// understand.
QString newId();

QString colorToHex(const QColor &color);
QColor colorFromHex(const QString &text, const QColor &fallback = QColor(Qt::white));

struct GradientStop {
    QColor color = Qt::white;
    double position = 0.0;
};

struct Fill {
    QString kind = QStringLiteral("none"); // none | solid | linearGradient | media
    QColor color = QColor(Qt::transparent);
    double gradientAngle = 0.0;
    QVector<GradientStop> stops;
    QString mediaId;
    QString mediaScaleMode = QStringLiteral("fill");
    QJsonObject extra;

    bool visible() const;
    QJsonObject toJson() const;
    static Fill fromJson(const QJsonObject &object);
    static Fill solid(const QColor &color, double opacity = 1.0);
};

struct Stroke {
    bool enabled = false;
    QColor color = QColor(Qt::white);
    double width = 0.0;
    QString dashKind = QStringLiteral("solid");
    QJsonObject extra;

    QJsonObject toJson() const;
    static Stroke fromJson(const QJsonObject &object);
};

struct Shadow {
    bool enabled = false;
    QColor color = QColor(0, 0, 0, 160);
    double blurRadius = 0.0;
    QPointF offset{0.0, 8.0};
    QJsonObject extra;

    QJsonObject toJson() const;
    static Shadow fromJson(const QJsonObject &object);
};

struct Insets {
    double top = 0.0;
    double left = 0.0;
    double bottom = 0.0;
    double right = 0.0;
};

struct TextStyle {
    // Empty fontName / zero fontSize / colorSet == false inherit the theme.
    QString fontName;
    double fontSize = 0.0;
    bool colorSet = false;
    QColor color = Qt::white;
    double tracking = 0.0;
    double lineHeightMultiple = 1.05;
    QString horizontalAlignment = QStringLiteral("center");
    QString verticalAlignment = QStringLiteral("middle");
    QString textTransform = QStringLiteral("none");
    bool autoShrink = true;
    double minFontSize = 24.0;
    bool underline = false;
    bool strikethrough = false;
    Insets insets;
    Fill fill;
    Stroke outline;
    Shadow shadow;
    QJsonObject extra;

    QJsonObject toJson() const;
    static TextStyle fromJson(const QJsonObject &object);
};

struct SlideObject {
    QString id;
    QString objectKind = QStringLiteral("text"); // text | media | shape | liveInput
    QString name;
    QString text;
    bool hasFrame = false;
    QRectF frame{120.0, 120.0, 1680.0, 840.0};
    double rotationDegrees = 0.0;
    double opacity = 1.0;
    bool hidden = false;

    QString shapeKind = QStringLiteral("rectangle");
    double cornerRadius = 0.0;
    Fill fill;
    Stroke stroke;
    Shadow shadow;
    TextStyle textStyle;

    QString mediaId;
    QString mediaScaleMode = QStringLiteral("fill");
    bool loops = false;
    QString liveInputId;

    QJsonObject textLink;
    QJsonObject extra;

    bool isText() const { return objectKind == QStringLiteral("text"); }
    bool isShape() const { return objectKind == QStringLiteral("shape"); }
    bool isMedia() const { return objectKind == QStringLiteral("media"); }

    QJsonObject toJson() const;
    static SlideObject fromJson(const QJsonObject &object);
};

struct Transition {
    bool enabled = false;
    QString kind = QStringLiteral("dissolve");
    double durationSeconds = 0.4;
    QColor color = QColor(Qt::black);
    QJsonObject extra;

    QJsonObject toJson() const;
    static Transition fromJson(const QJsonObject &object);
};

struct Slide {
    QString id;
    QString name;
    QVector<SlideObject> objects;
    Fill backgroundFill;
    Transition transition;
    QString notes;
    QString sectionId;
    QJsonObject extra;

    int indexOfObject(const QString &objectId) const;
    QJsonObject toJson() const;
    static Slide fromJson(const QJsonObject &object);
};

struct Presentation {
    QString id;
    QString name;
    QString presentationKind = QStringLiteral("deck");
    QString themeId;
    QString folder;
    int canvasWidth = 1920;
    int canvasHeight = 1080;
    QVector<Slide> slides;
    Fill backgroundFill;
    Transition transition;
    QJsonObject extra;

    int indexOfSlide(const QString &slideId) const;
    bool isValid() const { return !id.isEmpty() && !name.isEmpty(); }
    QJsonObject toJson() const;
    static Presentation fromJson(const QJsonObject &object);
};

struct Theme {
    QString id;
    QString name;
    QString fontFamily = QStringLiteral("Segoe UI");
    double fontSize = 96.0;
    QString textColorHex = QStringLiteral("#FFFFFF");
    QString backgroundColorHex = QStringLiteral("#000000");
    QJsonObject extra;

    QColor textColor() const { return colorFromHex(textColorHex, Qt::white); }
    QColor backgroundColor() const { return colorFromHex(backgroundColorHex, Qt::black); }

    QJsonObject toJson() const;
    static Theme fromJson(const QJsonObject &object);
};

struct MediaItem {
    QString id;
    QString name;
    QString mediaKind = QStringLiteral("image"); // image | video
    QString classification = QStringLiteral("background");
    QString fileHash;
    QString fileName;
    QString folder;
    QJsonObject extra;

    QJsonObject toJson() const;
    static MediaItem fromJson(const QJsonObject &object);
};

struct Bundle {
    Presentation presentation;
    QVector<Theme> themes;
    QVector<MediaItem> media;
    QHash<QString, QByteArray> blobs; // keyed by file hash
};

Theme defaultTheme();
SlideObject makeTextObject(const QString &name, const QString &text, const QRectF &frame,
                           double fontSize = 72.0);
SlideObject makeShapeObject(const QString &name, const QString &shapeKind, const QRectF &frame,
                            const QColor &color);
Slide makeBlankSlide(int number);
Presentation makePresentation(const QString &name, const Theme &theme);
QString slidePlainText(const Slide &slide);

// Title/lyrics friendly text for presenter and stage displays.
QString presentationFolderLabel(const Presentation &presentation);

} // namespace mxu
