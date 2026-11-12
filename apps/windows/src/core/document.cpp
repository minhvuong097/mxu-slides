#include "core/document.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QUuid>

#include <algorithm>

namespace mxu {
namespace {

QString stringValue(const QJsonObject &object, const char *key, const QString &fallback = {})
{
    const QJsonValue value = object.value(QLatin1String(key));
    return value.isString() ? value.toString() : fallback;
}

double numberValue(const QJsonObject &object, const char *key, double fallback = 0.0)
{
    const QJsonValue value = object.value(QLatin1String(key));
    return value.isDouble() ? value.toDouble() : fallback;
}

bool boolValue(const QJsonObject &object, const char *key, bool fallback = false)
{
    const QJsonValue value = object.value(QLatin1String(key));
    return value.isBool() ? value.toBool() : fallback;
}

QJsonObject objectValue(const QJsonObject &object, const char *key)
{
    const QJsonValue value = object.value(QLatin1String(key));
    return value.isObject() ? value.toObject() : QJsonObject();
}

QJsonArray arrayValue(const QJsonObject &object, const char *key)
{
    const QJsonValue value = object.value(QLatin1String(key));
    return value.isArray() ? value.toArray() : QJsonArray();
}

void setIfNotEmpty(QJsonObject &object, const char *key, const QString &value)
{
    if (value.isEmpty())
        object.remove(QLatin1String(key));
    else
        object.insert(QLatin1String(key), value);
}

void setDefault(QJsonObject &object, const char *key, const QJsonValue &value)
{
    if (!object.contains(QLatin1String(key)))
        object.insert(QLatin1String(key), value);
}

QString dashPatternKind(const QString &kind)
{
    return kind;
}

QJsonObject mergeExtra(QJsonObject extra)
{
    return extra;
}

} // namespace

QString newId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString colorToHex(const QColor &color)
{
    if (!color.isValid())
        return {};
    QColor rgb = color.toRgb();
    QString hex = rgb.name(QColor::HexRgb).toUpper();
    if (rgb.alpha() < 255)
        hex += QStringLiteral("%1").arg(rgb.alpha(), 2, 16, QLatin1Char('0')).toUpper();
    return hex;
}

QColor colorFromHex(const QString &text, const QColor &fallback)
{
    QString value = text.trimmed();
    if (value.isEmpty())
        return fallback;
    if (value.compare(QLatin1String("transparent"), Qt::CaseInsensitive) == 0)
        return QColor(0, 0, 0, 0);
    if (value.startsWith(QLatin1Char('#')))
        value.remove(0, 1);

    if (value.size() == 3) {
        value = QStringLiteral("%1%1%2%2%3%3")
                    .arg(value.at(0))
                    .arg(value.at(1))
                    .arg(value.at(2));
    }
    if (value.size() == 6 || value.size() == 8) {
        bool ok = false;
        const int red = value.mid(0, 2).toInt(&ok, 16);
        if (!ok)
            return fallback;
        const int green = value.mid(2, 2).toInt(&ok, 16);
        if (!ok)
            return fallback;
        const int blue = value.mid(4, 2).toInt(&ok, 16);
        if (!ok)
            return fallback;
        int alpha = 255;
        if (value.size() == 8) {
            alpha = value.mid(6, 2).toInt(&ok, 16);
            if (!ok)
                return fallback;
        }
        return QColor(red, green, blue, alpha);
    }
    const QColor named(value);
    return named.isValid() ? named : fallback;
}

bool Fill::visible() const
{
    if (kind == QLatin1String("solid"))
        return color.alpha() > 0;
    if (kind == QLatin1String("linearGradient"))
        return !stops.isEmpty();
    if (kind == QLatin1String("media"))
        return !mediaId.isEmpty();
    return false;
}

Fill Fill::solid(const QColor &color, double opacity)
{
    Fill fill;
    fill.kind = QStringLiteral("solid");
    fill.color = color;
    fill.color.setAlphaF(std::clamp(opacity, 0.0, 1.0) * color.alphaF());
    return fill;
}

QJsonObject Fill::toJson() const
{
    QJsonObject object = mergeExtra(extra);
    object.insert(QStringLiteral("fillKind"), kind);
    if (kind == QLatin1String("solid")) {
        object.insert(QStringLiteral("colorHex"), colorToHex(color));
    } else if (kind == QLatin1String("linearGradient")) {
        object.insert(QStringLiteral("gradientAngleDegrees"), gradientAngle);
        QJsonArray stopsArray;
        for (const GradientStop &stop : stops) {
            QJsonObject entry;
            entry.insert(QStringLiteral("colorHex"), colorToHex(stop.color));
            entry.insert(QStringLiteral("position"), stop.position);
            stopsArray.append(entry);
        }
        object.insert(QStringLiteral("gradientStops"), stopsArray);
    } else if (kind == QLatin1String("media")) {
        setIfNotEmpty(object, "mediaId", mediaId);
        object.insert(QStringLiteral("mediaScaleMode"), mediaScaleMode);
    }
    return object;
}

Fill Fill::fromJson(const QJsonObject &object)
{
    Fill fill;
    fill.extra = object;
    fill.kind = stringValue(object, "fillKind", QStringLiteral("none"));
    fill.color = colorFromHex(stringValue(object, "colorHex"), QColor(Qt::transparent));
    fill.gradientAngle = numberValue(object, "gradientAngleDegrees");
    const QJsonArray stopsArray = arrayValue(object, "gradientStops");
    for (const QJsonValue &entry : stopsArray) {
        const QJsonObject stopObject = entry.toObject();
        GradientStop stop;
        stop.color = colorFromHex(stringValue(stopObject, "colorHex"), Qt::white);
        stop.position = numberValue(stopObject, "position");
        fill.stops.append(stop);
    }
    fill.mediaId = stringValue(object, "mediaId");
    fill.mediaScaleMode = stringValue(object, "mediaScaleMode", QStringLiteral("fill"));
    return fill;
}

QJsonObject Stroke::toJson() const
{
    QJsonObject object = mergeExtra(extra);
    object.insert(QStringLiteral("colorHex"), colorToHex(color));
    object.insert(QStringLiteral("width"), width);
    object.insert(QStringLiteral("dashKind"), dashPatternKind(dashKind));
    return object;
}

Stroke Stroke::fromJson(const QJsonObject &object)
{
    Stroke stroke;
    stroke.extra = object;
    stroke.color = colorFromHex(stringValue(object, "colorHex"), Qt::white);
    stroke.width = numberValue(object, "width");
    stroke.dashKind = stringValue(object, "dashKind", QStringLiteral("solid"));
    stroke.enabled = stroke.width > 0.0;
    return stroke;
}

QJsonObject Shadow::toJson() const
{
    QJsonObject object = mergeExtra(extra);
    object.insert(QStringLiteral("colorHex"), colorToHex(color));
    object.insert(QStringLiteral("blurRadius"), blurRadius);
    object.insert(QStringLiteral("offsetX"), offset.x());
    object.insert(QStringLiteral("offsetY"), offset.y());
    return object;
}

Shadow Shadow::fromJson(const QJsonObject &object)
{
    Shadow shadow;
    shadow.extra = object;
    shadow.color = colorFromHex(stringValue(object, "colorHex"), QColor(0, 0, 0, 160));
    shadow.blurRadius = numberValue(object, "blurRadius");
    shadow.offset = QPointF(numberValue(object, "offsetX"), numberValue(object, "offsetY"));
    shadow.enabled = object.contains(QStringLiteral("colorHex"));
    return shadow;
}

QJsonObject TextStyle::toJson() const
{
    QJsonObject object = mergeExtra(extra);
    setIfNotEmpty(object, "fontName", fontName);
    if (fontSize > 0.0)
        object.insert(QStringLiteral("fontSize"), fontSize);
    else
        object.remove(QStringLiteral("fontSize"));
    if (colorSet)
        object.insert(QStringLiteral("colorHex"), colorToHex(color));
    else
        object.remove(QStringLiteral("colorHex"));

    object.insert(QStringLiteral("tracking"), tracking);
    object.insert(QStringLiteral("lineHeightMultiple"), lineHeightMultiple);
    object.insert(QStringLiteral("horizontalAlignment"), horizontalAlignment);
    object.insert(QStringLiteral("verticalAlignment"), verticalAlignment);
    object.insert(QStringLiteral("textTransform"), textTransform);
    object.insert(QStringLiteral("autoShrink"), autoShrink);
    object.insert(QStringLiteral("minFontSize"), minFontSize);
    if (underline)
        object.insert(QStringLiteral("underline"), true);
    else
        object.remove(QStringLiteral("underline"));
    if (strikethrough)
        object.insert(QStringLiteral("strikethrough"), true);
    else
        object.remove(QStringLiteral("strikethrough"));

    if (insets.top != 0.0)
        object.insert(QStringLiteral("insetTop"), insets.top);
    else
        object.remove(QStringLiteral("insetTop"));
    if (insets.left != 0.0)
        object.insert(QStringLiteral("insetLeft"), insets.left);
    else
        object.remove(QStringLiteral("insetLeft"));
    if (insets.bottom != 0.0)
        object.insert(QStringLiteral("insetBottom"), insets.bottom);
    else
        object.remove(QStringLiteral("insetBottom"));
    if (insets.right != 0.0)
        object.insert(QStringLiteral("insetRight"), insets.right);
    else
        object.remove(QStringLiteral("insetRight"));

    if (fill.visible())
        object.insert(QStringLiteral("fill"), fill.toJson());
    if (outline.enabled && outline.width > 0.0)
        object.insert(QStringLiteral("outline"), outline.toJson());
    if (shadow.enabled)
        object.insert(QStringLiteral("shadow"), shadow.toJson());
    return object;
}

TextStyle TextStyle::fromJson(const QJsonObject &object)
{
    TextStyle style;
    style.extra = object;
    style.fontName = stringValue(object, "fontName");
    style.fontSize = numberValue(object, "fontSize");
    style.colorSet = object.contains(QStringLiteral("colorHex"));
    style.color = colorFromHex(stringValue(object, "colorHex"), Qt::white);
    style.tracking = numberValue(object, "tracking");
    style.lineHeightMultiple = numberValue(object, "lineHeightMultiple", 1.05);
    style.horizontalAlignment = stringValue(object, "horizontalAlignment", QStringLiteral("center"));
    style.verticalAlignment = stringValue(object, "verticalAlignment", QStringLiteral("middle"));
    style.textTransform = stringValue(object, "textTransform", QStringLiteral("none"));
    style.autoShrink = boolValue(object, "autoShrink", true);
    style.minFontSize = numberValue(object, "minFontSize", 24.0);
    style.underline = boolValue(object, "underline");
    style.strikethrough = boolValue(object, "strikethrough");
    style.insets.top = numberValue(object, "insetTop");
    style.insets.left = numberValue(object, "insetLeft");
    style.insets.bottom = numberValue(object, "insetBottom");
    style.insets.right = numberValue(object, "insetRight");
    if (object.contains(QStringLiteral("fill")))
        style.fill = Fill::fromJson(objectValue(object, "fill"));
    if (object.contains(QStringLiteral("outline")))
        style.outline = Stroke::fromJson(objectValue(object, "outline"));
    if (object.contains(QStringLiteral("shadow")))
        style.shadow = Shadow::fromJson(objectValue(object, "shadow"));
    return style;
}

QJsonObject SlideObject::toJson() const
{
    QJsonObject object = mergeExtra(extra);
    object.insert(QStringLiteral("id"), id);
    object.insert(QStringLiteral("objectKind"), objectKind);
    object.insert(QStringLiteral("name"), name);
    object.insert(QStringLiteral("text"), text);

    if (hasFrame) {
        object.insert(QStringLiteral("x"), frame.x());
        object.insert(QStringLiteral("y"), frame.y());
        object.insert(QStringLiteral("width"), frame.width());
        object.insert(QStringLiteral("height"), frame.height());
    }
    if (rotationDegrees != 0.0 || object.contains(QStringLiteral("rotationDegrees")))
        object.insert(QStringLiteral("rotationDegrees"), rotationDegrees);
    if (opacity < 1.0)
        object.insert(QStringLiteral("opacity"), opacity);
    else
        object.remove(QStringLiteral("opacity"));
    if (hidden)
        object.insert(QStringLiteral("hidden"), true);
    else
        object.remove(QStringLiteral("hidden"));

    if (isShape()) {
        object.insert(QStringLiteral("shapeKind"), shapeKind);
        object.insert(QStringLiteral("cornerRadius"), cornerRadius);
    }
    if (fill.visible())
        object.insert(QStringLiteral("fill"), fill.toJson());
    else
        object.remove(QStringLiteral("fill"));
    if (stroke.enabled && stroke.width > 0.0)
        object.insert(QStringLiteral("stroke"), stroke.toJson());
    else
        object.remove(QStringLiteral("stroke"));
    if (shadow.enabled)
        object.insert(QStringLiteral("shadow"), shadow.toJson());
    else
        object.remove(QStringLiteral("shadow"));

    if (isText()) {
        const QJsonObject style = textStyle.toJson();
        if (!style.isEmpty())
            object.insert(QStringLiteral("textStyle"), style);
        else
            object.remove(QStringLiteral("textStyle"));
    }
    if (isMedia()) {
        setIfNotEmpty(object, "mediaId", mediaId);
        object.insert(QStringLiteral("mediaScaleMode"), mediaScaleMode);
        if (loops)
            object.insert(QStringLiteral("loops"), true);
        else
            object.remove(QStringLiteral("loops"));
    }
    if (objectKind == QLatin1String("liveInput"))
        setIfNotEmpty(object, "liveInputId", liveInputId);
    if (!textLink.isEmpty())
        object.insert(QStringLiteral("textLink"), textLink);
    return object;
}

SlideObject SlideObject::fromJson(const QJsonObject &object)
{
    SlideObject slideObject;
    slideObject.extra = object;
    slideObject.id = stringValue(object, "id", newId());
    slideObject.objectKind = stringValue(object, "objectKind", QStringLiteral("text"));
    slideObject.name = stringValue(object, "name", QStringLiteral("Object"));
    slideObject.text = stringValue(object, "text");

    slideObject.hasFrame = object.contains(QStringLiteral("x"))
        || object.contains(QStringLiteral("y"))
        || object.contains(QStringLiteral("width"))
        || object.contains(QStringLiteral("height"));
    slideObject.frame = QRectF(numberValue(object, "x", 120.0),
                               numberValue(object, "y", 120.0),
                               numberValue(object, "width", 1680.0),
                               numberValue(object, "height", 840.0));
    slideObject.rotationDegrees = numberValue(object, "rotationDegrees");
    slideObject.opacity = numberValue(object, "opacity", 1.0);
    slideObject.hidden = boolValue(object, "hidden");

    slideObject.shapeKind = stringValue(object, "shapeKind", QStringLiteral("rectangle"));
    slideObject.cornerRadius = numberValue(object, "cornerRadius");
    if (object.contains(QStringLiteral("fill")))
        slideObject.fill = Fill::fromJson(objectValue(object, "fill"));
    if (object.contains(QStringLiteral("stroke")))
        slideObject.stroke = Stroke::fromJson(objectValue(object, "stroke"));
    if (object.contains(QStringLiteral("shadow")))
        slideObject.shadow = Shadow::fromJson(objectValue(object, "shadow"));
    if (object.contains(QStringLiteral("textStyle")))
        slideObject.textStyle = TextStyle::fromJson(objectValue(object, "textStyle"));

    slideObject.mediaId = stringValue(object, "mediaId");
    slideObject.mediaScaleMode = stringValue(object, "mediaScaleMode", QStringLiteral("fill"));
    slideObject.loops = boolValue(object, "loops");
    slideObject.liveInputId = stringValue(object, "liveInputId");
    slideObject.textLink = objectValue(object, "textLink");
    return slideObject;
}

QJsonObject Transition::toJson() const
{
    QJsonObject object = mergeExtra(extra);
    object.insert(QStringLiteral("transitionKind"), kind);
    object.insert(QStringLiteral("durationSeconds"), durationSeconds);
    if (kind == QLatin1String("fadeColor") || color.alpha() > 0)
        object.insert(QStringLiteral("colorHex"), colorToHex(color));
    return object;
}

Transition Transition::fromJson(const QJsonObject &object)
{
    Transition transition;
    transition.extra = object;
    transition.enabled = !object.isEmpty();
    transition.kind = stringValue(object, "transitionKind", QStringLiteral("dissolve"));
    transition.durationSeconds = numberValue(object, "durationSeconds", 0.4);
    transition.color = colorFromHex(stringValue(object, "colorHex"), Qt::black);
    return transition;
}

int Slide::indexOfObject(const QString &objectId) const
{
    for (int index = 0; index < objects.size(); ++index) {
        if (objects.at(index).id == objectId)
            return index;
    }
    return -1;
}

QJsonObject Slide::toJson() const
{
    QJsonObject object = mergeExtra(extra);
    object.insert(QStringLiteral("id"), id);
    object.insert(QStringLiteral("name"), name);
    QJsonArray objectArray;
    for (const SlideObject &slideObject : objects)
        objectArray.append(slideObject.toJson());
    object.insert(QStringLiteral("objects"), objectArray);
    if (backgroundFill.visible())
        object.insert(QStringLiteral("backgroundFill"), backgroundFill.toJson());
    else
        object.remove(QStringLiteral("backgroundFill"));
    if (transition.enabled)
        object.insert(QStringLiteral("transition"), transition.toJson());
    else
        object.remove(QStringLiteral("transition"));
    setIfNotEmpty(object, "notes", notes);
    setIfNotEmpty(object, "sectionId", sectionId);
    return object;
}

Slide Slide::fromJson(const QJsonObject &object)
{
    Slide slide;
    slide.extra = object;
    slide.id = stringValue(object, "id", newId());
    slide.name = stringValue(object, "name", QStringLiteral("Slide"));
    const QJsonArray objectArray = arrayValue(object, "objects");
    for (const QJsonValue &entry : objectArray)
        slide.objects.append(SlideObject::fromJson(entry.toObject()));
    if (object.contains(QStringLiteral("backgroundFill")))
        slide.backgroundFill = Fill::fromJson(objectValue(object, "backgroundFill"));
    if (object.contains(QStringLiteral("transition")))
        slide.transition = Transition::fromJson(objectValue(object, "transition"));
    slide.notes = stringValue(object, "notes");
    slide.sectionId = stringValue(object, "sectionId");
    return slide;
}

int Presentation::indexOfSlide(const QString &slideId) const
{
    for (int index = 0; index < slides.size(); ++index) {
        if (slides.at(index).id == slideId)
            return index;
    }
    return -1;
}

QJsonObject Presentation::toJson() const
{
    QJsonObject object = mergeExtra(extra);
    object.insert(QStringLiteral("id"), id);
    object.insert(QStringLiteral("name"), name);
    object.insert(QStringLiteral("presentationKind"), presentationKind);
    object.insert(QStringLiteral("themeId"), themeId);
    object.insert(QStringLiteral("canvasWidth"), canvasWidth);
    object.insert(QStringLiteral("canvasHeight"), canvasHeight);
    QJsonArray slideArray;
    for (const Slide &slide : slides)
        slideArray.append(slide.toJson());
    object.insert(QStringLiteral("slides"), slideArray);
    setIfNotEmpty(object, "folder", folder);
    if (backgroundFill.visible())
        object.insert(QStringLiteral("backgroundFill"), backgroundFill.toJson());
    if (transition.enabled)
        object.insert(QStringLiteral("transition"), transition.toJson());
    return object;
}

Presentation Presentation::fromJson(const QJsonObject &object)
{
    Presentation presentation;
    presentation.extra = object;
    presentation.id = stringValue(object, "id", newId());
    presentation.name = stringValue(object, "name", QStringLiteral("Untitled"));
    presentation.presentationKind = stringValue(object, "presentationKind", QStringLiteral("deck"));
    presentation.themeId = stringValue(object, "themeId");
    presentation.folder = stringValue(object, "folder");
    presentation.canvasWidth = std::max(1, object.value(QStringLiteral("canvasWidth")).toInt(1920));
    presentation.canvasHeight = std::max(1, object.value(QStringLiteral("canvasHeight")).toInt(1080));
    const QJsonArray slideArray = arrayValue(object, "slides");
    for (const QJsonValue &entry : slideArray)
        presentation.slides.append(Slide::fromJson(entry.toObject()));
    if (object.contains(QStringLiteral("backgroundFill")))
        presentation.backgroundFill = Fill::fromJson(objectValue(object, "backgroundFill"));
    if (object.contains(QStringLiteral("transition")))
        presentation.transition = Transition::fromJson(objectValue(object, "transition"));
    return presentation;
}

QJsonObject Theme::toJson() const
{
    QJsonObject object = mergeExtra(extra);
    object.insert(QStringLiteral("id"), id);
    object.insert(QStringLiteral("name"), name);
    object.insert(QStringLiteral("fontFamily"), fontFamily);
    object.insert(QStringLiteral("fontSize"), fontSize);
    object.insert(QStringLiteral("textColorHex"), textColorHex);
    object.insert(QStringLiteral("backgroundColorHex"), backgroundColorHex);
    return object;
}

Theme Theme::fromJson(const QJsonObject &object)
{
    Theme theme;
    theme.extra = object;
    theme.id = stringValue(object, "id", newId());
    theme.name = stringValue(object, "name", QStringLiteral("Theme"));
    theme.fontFamily = stringValue(object, "fontFamily", QStringLiteral("Segoe UI"));
    theme.fontSize = numberValue(object, "fontSize", 96.0);
    theme.textColorHex = stringValue(object, "textColorHex", QStringLiteral("#FFFFFF"));
    theme.backgroundColorHex = stringValue(object, "backgroundColorHex", QStringLiteral("#000000"));
    return theme;
}

QJsonObject MediaItem::toJson() const
{
    QJsonObject object = mergeExtra(extra);
    object.insert(QStringLiteral("id"), id);
    object.insert(QStringLiteral("name"), name);
    object.insert(QStringLiteral("mediaKind"), mediaKind);
    object.insert(QStringLiteral("classification"), classification);
    object.insert(QStringLiteral("fileHash"), fileHash);
    object.insert(QStringLiteral("fileName"), fileName);
    setDefault(object, "fileStatus", QStringLiteral("ready"));
    setDefault(object, "statusDetail", QString());
    setDefault(object, "tags", QJsonArray());
    setDefault(object, "favorite", false);
    setDefault(object, "collections", QJsonArray());
    setDefault(object, "loops", false);
    setIfNotEmpty(object, "folder", folder);
    return object;
}

MediaItem MediaItem::fromJson(const QJsonObject &object)
{
    MediaItem item;
    item.extra = object;
    item.id = stringValue(object, "id", newId());
    item.name = stringValue(object, "name", stringValue(object, "fileName"));
    item.mediaKind = stringValue(object, "mediaKind", QStringLiteral("image"));
    item.classification = stringValue(object, "classification", QStringLiteral("background"));
    item.fileHash = stringValue(object, "fileHash");
    item.fileName = stringValue(object, "fileName", item.name);
    item.folder = stringValue(object, "folder");
    return item;
}

Theme defaultTheme()
{
    Theme theme;
    theme.id = QStringLiteral("house-theme");
    theme.name = QStringLiteral("House");
    theme.fontFamily = QStringLiteral("Segoe UI");
    theme.fontSize = 96.0;
    theme.textColorHex = QStringLiteral("#FFFFFF");
    theme.backgroundColorHex = QStringLiteral("#101214");
    return theme;
}

SlideObject makeTextObject(const QString &name, const QString &text, const QRectF &frame,
                           double fontSize)
{
    SlideObject object;
    object.id = newId();
    object.objectKind = QStringLiteral("text");
    object.name = name;
    object.text = text;
    object.hasFrame = true;
    object.frame = frame;
    object.textStyle.fontSize = fontSize;
    object.textStyle.autoShrink = true;
    object.textStyle.verticalAlignment = QStringLiteral("middle");
    object.textStyle.horizontalAlignment = QStringLiteral("center");
    return object;
}

SlideObject makeShapeObject(const QString &name, const QString &shapeKind, const QRectF &frame,
                            const QColor &color)
{
    SlideObject object;
    object.id = newId();
    object.objectKind = QStringLiteral("shape");
    object.name = name;
    object.hasFrame = true;
    object.frame = frame;
    object.shapeKind = shapeKind;
    object.fill = Fill::solid(color);
    return object;
}

Slide makeBlankSlide(int number)
{
    Slide slide;
    slide.id = newId();
    slide.name = QStringLiteral("Slide %1").arg(number);
    return slide;
}

Presentation makePresentation(const QString &name, const Theme &theme)
{
    Presentation presentation;
    presentation.id = newId();
    presentation.name = name;
    presentation.themeId = theme.id;
    presentation.canvasWidth = 1920;
    presentation.canvasHeight = 1080;
    presentation.slides.append(makeBlankSlide(1));
    return presentation;
}

QString slidePlainText(const Slide &slide)
{
    QStringList parts;
    for (const SlideObject &object : slide.objects) {
        if (object.isText() && !object.text.trimmed().isEmpty())
            parts.append(object.text.trimmed());
    }
    return parts.join(QStringLiteral("\n"));
}

QString presentationFolderLabel(const Presentation &presentation)
{
    return presentation.folder.isEmpty() ? QStringLiteral("Unsorted") : presentation.folder;
}

} // namespace mxu
