#pragma once

#include "core/document.h"

#include <QHash>
#include <QImage>
#include <QSize>
#include <QTime>

#include <functional>

class QPainter;
class QRectF;

namespace mxu {

struct RenderContext {
    QTime clock = QTime::currentTime();
    int timerSeconds = -1;
    int slideIndex = 0;
    int slideCount = 0;
    QString presentationName;
};

class SlideRenderer {
public:
    using MediaResolver = std::function<QString(const QString &mediaId)>;

    void setMediaResolver(MediaResolver resolver);
    void clearCache();

    void paint(QPainter &painter, const Presentation &presentation, const Slide &slide,
               const Theme &theme, const QRectF &target, const RenderContext &context) const;

    QImage render(const Presentation &presentation, const Slide &slide, const Theme &theme,
                  const QSize &size, const RenderContext &context = {}) const;

private:
    QImage loadImage(const QString &path) const;
    QImage mediaImage(const QString &mediaId) const;
    void paintBackground(QPainter &painter, const Presentation &presentation, const Slide &slide,
                         const Theme &theme) const;
    void paintObject(QPainter &painter, const SlideObject &object, const Presentation &presentation,
                     const Theme &theme, const RenderContext &context) const;
    void paintFill(QPainter &painter, const QRectF &rect, const QPainterPath &path,
                   const Fill &fill) const;
    void paintText(QPainter &painter, const SlideObject &object, const Presentation &presentation,
                   const Theme &theme, const RenderContext &context) const;
    void drawImageScaled(QPainter &painter, const QRectF &target, const QImage &image,
                         const QString &scaleMode) const;
    QString resolvedText(const SlideObject &object, const RenderContext &context) const;

    MediaResolver m_mediaResolver;
    mutable QHash<QString, QImage> m_imageCache;
};

} // namespace mxu
