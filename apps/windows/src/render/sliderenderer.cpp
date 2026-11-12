#include "render/sliderenderer.h"

#include <QFileInfo>
#include <QFontMetricsF>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QSet>
#include <QtMath>

#include <algorithm>

namespace mxu {
namespace {

QRectF effectiveFrame(const SlideObject &object, const Presentation &presentation)
{
    if (object.hasFrame && object.frame.width() > 0.0 && object.frame.height() > 0.0)
        return object.frame;
    return QRectF(presentation.canvasWidth * 0.06, presentation.canvasHeight * 0.16,
                  presentation.canvasWidth * 0.88, presentation.canvasHeight * 0.66);
}

bool looksLikeVideo(const QString &path)
{
    static const QSet<QString> extensions{
        QStringLiteral("mp4"), QStringLiteral("mov"), QStringLiteral("m4v"), QStringLiteral("webm"),
        QStringLiteral("avi"), QStringLiteral("mkv"), QStringLiteral("mts"), QStringLiteral("m2ts")};
    return extensions.contains(QFileInfo(path).suffix().toLower());
}

QFont fontFor(const TextStyle &style, const Theme &theme, int pixelSize)
{
    QFont font(style.fontName.isEmpty() ? theme.fontFamily : style.fontName);
    font.setPixelSize(std::max(1, pixelSize));
    font.setLetterSpacing(QFont::AbsoluteSpacing, style.tracking);
    font.setUnderline(style.underline);
    font.setStrikeOut(style.strikethrough);
    return font;
}

QColor textColorFor(const TextStyle &style, const Theme &theme)
{
    return style.colorSet ? style.color : theme.textColor();
}

QBrush fillBrush(const Fill &fill, const QRectF &rect, const QColor &fallback)
{
    if (fill.kind == QLatin1String("solid") && fill.color.alpha() > 0)
        return QBrush(fill.color);
    if (fill.kind == QLatin1String("linearGradient") && !fill.stops.isEmpty()) {
        const qreal radians = qDegreesToRadians(fill.gradientAngle);
        const QPointF direction(std::cos(radians), std::sin(radians));
        const qreal length = std::hypot(rect.width(), rect.height()) / 2.0;
        const QPointF center = rect.center();
        QLinearGradient gradient(center - direction * length, center + direction * length);
        for (const GradientStop &stop : fill.stops) {
            gradient.setColorAt(std::clamp(stop.position, 0.0, 1.0), stop.color);
        }
        return QBrush(gradient);
    }
    return QBrush(fallback);
}

QVector<QString> wrapParagraph(const QString &paragraph, const QFontMetricsF &metrics, qreal maxWidth)
{
    QVector<QString> lines;
    if (paragraph.isEmpty()) {
        lines.append(QString());
        return lines;
    }
    const QStringList words = paragraph.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (words.isEmpty()) {
        lines.append(QString());
        return lines;
    }

    QString current;
    for (const QString &word : words) {
        const QString candidate = current.isEmpty() ? word : current + QLatin1Char(' ') + word;
        if (metrics.horizontalAdvance(candidate) <= maxWidth) {
            current = candidate;
            continue;
        }
        if (!current.isEmpty()) {
            lines.append(current);
            current.clear();
        }
        QString remaining = word;
        while (metrics.horizontalAdvance(remaining) > maxWidth && remaining.size() > 1) {
            int take = remaining.size();
            while (take > 1 && metrics.horizontalAdvance(remaining.left(take)) > maxWidth)
                --take;
            lines.append(remaining.left(take));
            remaining = remaining.mid(take);
        }
        current = remaining;
    }
    if (!current.isEmpty())
        lines.append(current);
    if (lines.isEmpty())
        lines.append(QString());
    return lines;
}

struct TextLayout {
    QVector<QString> lines;
    QFont font;
    qreal lineSpacing = 0.0;
    qreal maxLineWidth = 0.0;
    qreal height = 0.0;
    qreal ascent = 0.0;
    qreal fontHeight = 0.0;
};

TextLayout buildLayout(const QString &text, const TextStyle &style, const Theme &theme, qreal maxWidth,
                       qreal maxHeight)
{
    const qreal baseSize = style.fontSize > 0.0 ? style.fontSize : theme.fontSize;
    const qreal minSize = std::max(1.0, std::min(style.minFontSize > 0.0 ? style.minFontSize : 24.0,
                                              baseSize));
    TextLayout layout;
    int size = std::max(1, qRound(baseSize));

    const auto build = [&](int pixelSize) {
        TextLayout candidate;
        candidate.font = fontFor(style, theme, pixelSize);
        const QFontMetricsF metrics(candidate.font);
        const QStringList paragraphs = text.split(QLatin1Char('\n'));
        for (const QString &paragraph : paragraphs)
            candidate.lines += wrapParagraph(paragraph, metrics, std::max(1.0, maxWidth));
        candidate.lineSpacing = metrics.height() * std::max(0.2, style.lineHeightMultiple);
        candidate.fontHeight = metrics.height();
        candidate.ascent = metrics.ascent();
        candidate.height = candidate.lineSpacing * candidate.lines.size();
        for (const QString &line : candidate.lines)
            candidate.maxLineWidth = std::max(candidate.maxLineWidth, metrics.horizontalAdvance(line));
        return candidate;
    };

    layout = build(size);
    if (style.autoShrink) {
        while ((layout.height > maxHeight + 0.5 || layout.maxLineWidth > maxWidth + 0.5)
               && size > minSize) {
            size = std::max(int(minSize), size - 2);
            layout = build(size);
        }
    }
    return layout;
}

} // namespace

void SlideRenderer::setMediaResolver(MediaResolver resolver)
{
    m_mediaResolver = std::move(resolver);
    m_imageCache.clear();
}

void SlideRenderer::clearCache()
{
    m_imageCache.clear();
}

QImage SlideRenderer::loadImage(const QString &path) const
{
    if (path.isEmpty())
        return {};
    const auto cached = m_imageCache.constFind(path);
    if (cached != m_imageCache.constEnd())
        return cached.value();
    QImage image(path);
    if (image.isNull())
        m_imageCache.insert(path, QImage());
    else
        m_imageCache.insert(path, image.convertToFormat(QImage::Format_ARGB32_Premultiplied));
    return m_imageCache.value(path);
}

QImage SlideRenderer::mediaImage(const QString &mediaId) const
{
    if (mediaId.isEmpty() || !m_mediaResolver)
        return {};
    const QString path = m_mediaResolver(mediaId);
    if (path.isEmpty() || looksLikeVideo(path))
        return {};
    return loadImage(path);
}

void SlideRenderer::drawImageScaled(QPainter &painter, const QRectF &target, const QImage &image,
                                    const QString &scaleMode) const
{
    if (image.isNull() || target.width() <= 0.0 || target.height() <= 0.0)
        return;
    painter.save();
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const QSizeF imageSize(image.size());
    if (scaleMode == QLatin1String("stretch")) {
        painter.drawImage(target, image);
    } else if (scaleMode == QLatin1String("fit")) {
        const QSizeF scaled = imageSize.scaled(target.size(), Qt::KeepAspectRatio);
        const QRectF destination(target.center().x() - scaled.width() / 2.0,
                                 target.center().y() - scaled.height() / 2.0, scaled.width(),
                                 scaled.height());
        painter.drawImage(destination, image);
    } else {
        if (scaleMode == QLatin1String("scaleAndBlur")) {
            const QImage small = image.scaled(QSize(48, 48), Qt::KeepAspectRatioByExpanding,
                                              Qt::SmoothTransformation);
            painter.setClipRect(target);
            painter.drawImage(target, small.scaled(target.size().toSize(), Qt::IgnoreAspectRatio,
                                                   Qt::SmoothTransformation));
        }
        const QSizeF scaled = imageSize.scaled(target.size(), Qt::KeepAspectRatioByExpanding);
        const QRectF destination(target.center().x() - scaled.width() / 2.0,
                                 target.center().y() - scaled.height() / 2.0, scaled.width(),
                                 scaled.height());
        painter.setClipRect(target);
        painter.drawImage(destination, image);
        if (scaleMode == QLatin1String("scaleAndBlur")) {
            const QSizeF fitted = imageSize.scaled(target.size(), Qt::KeepAspectRatio);
            const QRectF fitRect(target.center().x() - fitted.width() / 2.0,
                                 target.center().y() - fitted.height() / 2.0, fitted.width(),
                                 fitted.height());
            painter.drawImage(fitRect, image);
        }
    }
    painter.restore();
}

void SlideRenderer::paintFill(QPainter &painter, const QRectF &rect, const QPainterPath &path,
                              const Fill &fill) const
{
    painter.save();
    if (fill.kind == QLatin1String("solid") && fill.color.alpha() > 0) {
        painter.fillPath(path, fill.color);
    } else if (fill.kind == QLatin1String("linearGradient") && !fill.stops.isEmpty()) {
        painter.fillPath(path, fillBrush(fill, rect, Qt::transparent));
    } else if (fill.kind == QLatin1String("media")) {
        painter.setClipPath(path);
        const QImage image = mediaImage(fill.mediaId);
        if (!image.isNull()) {
            drawImageScaled(painter, rect, image, fill.mediaScaleMode);
        } else {
            painter.fillPath(path, QColor(18, 23, 32));
        }
    }
    painter.restore();
}

void SlideRenderer::paintBackground(QPainter &painter, const Presentation &presentation,
                                    const Slide &slide, const Theme &theme) const
{
    const QRectF rect(0, 0, presentation.canvasWidth, presentation.canvasHeight);
    QPainterPath path;
    path.addRect(rect);

    Fill background = slide.backgroundFill.visible() ? slide.backgroundFill : presentation.backgroundFill;
    if (background.visible()) {
        paintFill(painter, rect, path, background);
        return;
    }
    painter.fillRect(rect, theme.backgroundColor());
}

void SlideRenderer::paint(QPainter &painter, const Presentation &presentation, const Slide &slide,
                          const Theme &theme, const QRectF &target, const RenderContext &context) const
{
    if (target.width() <= 0.0 || target.height() <= 0.0)
        return;
    const qreal width = presentation.canvasWidth > 0 ? presentation.canvasWidth : 1920;
    const qreal height = presentation.canvasHeight > 0 ? presentation.canvasHeight : 1080;

    painter.save();
    painter.setClipRect(target);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing
                           | QPainter::SmoothPixmapTransform);
    painter.translate(target.topLeft());
    painter.scale(target.width() / width, target.height() / height);
    paintBackground(painter, presentation, slide, theme);
    for (const SlideObject &object : slide.objects)
        paintObject(painter, object, presentation, theme, context);
    painter.restore();
}

void SlideRenderer::paintObject(QPainter &painter, const SlideObject &object,
                                const Presentation &presentation, const Theme &theme,
                                const RenderContext &context) const
{
    if (object.hidden || object.opacity <= 0.001)
        return;
    const QRectF frame = effectiveFrame(object, presentation);
    if (frame.width() <= 0.0 || frame.height() <= 0.0)
        return;

    painter.save();
    painter.setOpacity(painter.opacity() * std::clamp(object.opacity, 0.0, 1.0));
    const QPointF center = frame.center();
    painter.translate(center);
    painter.rotate(object.rotationDegrees);
    painter.translate(-center);

    if (object.isText()) {
        paintText(painter, object, presentation, theme, context);
    } else if (object.isShape()) {
        QPainterPath path;
        if (object.shapeKind == QLatin1String("ellipse")) {
            path.addEllipse(frame);
        } else if (object.shapeKind == QLatin1String("roundedRectangle")) {
            const qreal radius = std::clamp(object.cornerRadius, 0.0,
                                            std::min(frame.width(), frame.height()) / 2.0);
            path.addRoundedRect(frame, radius, radius);
        } else {
            path.addRect(frame);
        }

        if (object.shadow.enabled) {
            QPainterPath shadowPath = path.translated(object.shadow.offset);
            QColor shadowColor = object.shadow.color;
            painter.fillPath(shadowPath, shadowColor);
        }
        if (object.fill.visible())
            paintFill(painter, frame, path, object.fill);
        if (object.stroke.enabled && object.stroke.width > 0.0) {
            QPen pen(object.stroke.color, object.stroke.width);
            pen.setJoinStyle(Qt::RoundJoin);
            pen.setCapStyle(Qt::RoundCap);
            if (object.stroke.dashKind == QLatin1String("dashed"))
                pen.setDashPattern({4.0, 3.0});
            else if (object.stroke.dashKind == QLatin1String("dotted"))
                pen.setDashPattern({1.0, 2.0});
            painter.setPen(pen);
            painter.setBrush(Qt::NoBrush);
            painter.drawPath(path);
        }
    } else if (object.isMedia() || object.objectKind == QLatin1String("liveInput")) {
        const QImage image = mediaImage(object.mediaId);
        const QString path = object.mediaId.isEmpty() || !m_mediaResolver
            ? QString()
            : m_mediaResolver(object.mediaId);
        if (!image.isNull()) {
            painter.save();
            painter.setClipRect(frame);
            drawImageScaled(painter, frame, image, object.mediaScaleMode);
            painter.restore();
        } else {
            painter.setPen(QPen(QColor(130, 142, 160, 160), 2, Qt::DashLine));
            painter.setBrush(QColor(20, 25, 34, 220));
            painter.drawRoundedRect(frame, 12, 12);
            QFont label;
            label.setPixelSize(std::max(16, int(std::min(frame.height() / 6.0, 48.0))));
            painter.setFont(label);
            painter.setPen(QColor(150, 162, 180));
            const bool video = !path.isEmpty() && looksLikeVideo(path);
            painter.drawText(frame, Qt::AlignCenter,
                             video ? QStringLiteral("VIDEO\n%1").arg(object.name)
                                   : QStringLiteral("MEDIA OFFLINE\n%1").arg(object.name));
        }
    }
    painter.restore();
}

QString SlideRenderer::resolvedText(const SlideObject &object, const RenderContext &context) const
{
    const QString source = object.textLink.value(QStringLiteral("source")).toString();
    if (source == QLatin1String("clock")) {
        QString format = object.textLink.value(QStringLiteral("clockFormat")).toString();
        if (format.isEmpty())
            format = QStringLiteral("h:mm AP");
        format.replace(QLatin1Char('a'), QStringLiteral("AP"));
        return context.clock.toString(format);
    }
    if (source == QLatin1String("timer")) {
        if (context.timerSeconds < 0)
            return QStringLiteral("--:--");
        const int seconds = std::max(0, context.timerSeconds);
        return QStringLiteral("%1:%2")
            .arg(seconds / 60, 2, 10, QLatin1Char('0'))
            .arg(seconds % 60, 2, 10, QLatin1Char('0'));
    }
    if (source == QLatin1String("slidePosition")) {
        return QStringLiteral("%1 / %2").arg(context.slideIndex + 1).arg(std::max(1, context.slideCount));
    }
    if (source == QLatin1String("currentPresentation"))
        return context.presentationName.isEmpty() ? object.text : context.presentationName;
    return object.text;
}

void SlideRenderer::paintText(QPainter &painter, const SlideObject &object,
                              const Presentation &presentation, const Theme &theme,
                              const RenderContext &context) const
{
    QString text = resolvedText(object, context);
    if (text.isEmpty())
        return;
    if (object.textStyle.textTransform == QLatin1String("uppercase"))
        text = text.toUpper();

    QRectF frame = effectiveFrame(object, presentation);
    frame.adjust(object.textStyle.insets.left, object.textStyle.insets.top,
                 -object.textStyle.insets.right, -object.textStyle.insets.bottom);
    if (frame.width() <= 1.0 || frame.height() <= 1.0)
        frame = effectiveFrame(object, presentation);

    const TextLayout layout = buildLayout(text, object.textStyle, theme, frame.width(), frame.height());
    if (layout.lines.isEmpty())
        return;

    qreal top = frame.top();
    if (object.textStyle.verticalAlignment == QLatin1String("middle"))
        top = frame.top() + (frame.height() - layout.height) / 2.0;
    else if (object.textStyle.verticalAlignment == QLatin1String("bottom"))
        top = frame.bottom() - layout.height;
    top = std::max(top, frame.top());

    const QColor color = textColorFor(object.textStyle, theme);
    const QBrush brush = object.textStyle.fill.visible()
        ? fillBrush(object.textStyle.fill, frame, color)
        : QBrush(color);

    painter.setFont(layout.font);
    const qreal extraLeading = (layout.lineSpacing - layout.fontHeight) / 2.0;

    for (int index = 0; index < layout.lines.size(); ++index) {
        const QString &line = layout.lines.at(index);
        const QFontMetricsF metrics(layout.font);
        const qreal lineWidth = metrics.horizontalAdvance(line);
        qreal x = frame.left();
        if (object.textStyle.horizontalAlignment == QLatin1String("center"))
            x = frame.center().x() - lineWidth / 2.0;
        else if (object.textStyle.horizontalAlignment == QLatin1String("right"))
            x = frame.right() - lineWidth;
        const qreal baseline = top + index * layout.lineSpacing + layout.ascent + extraLeading;

        if (object.textStyle.shadow.enabled) {
            QColor shadow = object.textStyle.shadow.color;
            painter.setPen(QPen(shadow, 0));
            painter.drawText(QPointF(x + object.textStyle.shadow.offset.x(),
                                     baseline + object.textStyle.shadow.offset.y()),
                             line);
        }
        if (object.textStyle.outline.enabled && object.textStyle.outline.width > 0.0) {
            QPainterPath path;
            path.addText(QPointF(x, baseline), layout.font, line);
            painter.setPen(QPen(object.textStyle.outline.color, object.textStyle.outline.width));
            painter.setBrush(Qt::NoBrush);
            painter.drawPath(path);
        }
        painter.setPen(QPen(brush, 0));
        painter.drawText(QPointF(x, baseline), line);
    }
}

QImage SlideRenderer::render(const Presentation &presentation, const Slide &slide, const Theme &theme,
                             const QSize &size, const RenderContext &context) const
{
    if (size.width() <= 0 || size.height() <= 0)
        return {};
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    paint(painter, presentation, slide, theme, QRectF(QPointF(0, 0), QSizeF(size)), context);
    return image;
}

} // namespace mxu
