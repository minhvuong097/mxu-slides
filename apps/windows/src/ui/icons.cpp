#include "ui/icons.h"

#include "ui/theme.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>

namespace mxu::ui {

void paintGlyph(QPainter &painter, Glyph glyph, const QRectF &rect, const QColor &color,
                qreal strokeWidth)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.translate(rect.topLeft());
    const qreal scale = qMin(rect.width(), rect.height()) / 24.0;
    painter.scale(scale, scale);
    painter.translate((rect.width() / scale - 24.0) / 2.0,
                      (rect.height() / scale - 24.0) / 2.0);

    QPen pen(color, strokeWidth);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    const auto line = [&](qreal x1, qreal y1, qreal x2, qreal y2) {
        painter.drawLine(QPointF(x1, y1), QPointF(x2, y2));
    };
    const auto arrowHead = [&](qreal x, qreal y, qreal dx, qreal dy) {
        line(x, y, x + dx, y + dy);
        line(x, y, x - dy, y + dx);
    };

    switch (glyph) {
    case Glyph::Plus:
        line(12, 5, 12, 19);
        line(5, 12, 19, 12);
        break;
    case Glyph::Import:
        painter.drawRoundedRect(QRectF(4, 15, 16, 5), 1.5, 1.5);
        line(12, 4, 12, 14);
        arrowHead(12, 14, -4, -4);
        break;
    case Glyph::Export:
        painter.drawRoundedRect(QRectF(4, 15, 16, 5), 1.5, 1.5);
        line(12, 18, 12, 8);
        arrowHead(12, 8, -4, 4);
        break;
    case Glyph::Undo: {
        QPainterPath path;
        path.moveTo(9, 7);
        path.lineTo(4, 12);
        path.lineTo(9, 17);
        path.moveTo(4, 12);
        path.lineTo(13, 12);
        path.cubicTo(19, 12, 20, 19, 14, 19);
        painter.drawPath(path);
        break;
    }
    case Glyph::Redo:
        painter.save();
        painter.translate(24, 0);
        painter.scale(-1, 1);
        paintGlyph(painter, Glyph::Undo, QRectF(0, 0, 24, 24), color, strokeWidth);
        painter.restore();
        break;
    case Glyph::Settings: {
        painter.drawEllipse(QPointF(12, 12), 3.2, 3.2);
        for (int index = 0; index < 8; ++index) {
            const qreal angle = qDegreesToRadians(index * 45.0);
            const QPointF inner(12 + std::cos(angle) * 6.6, 12 + std::sin(angle) * 6.6);
            const QPointF outer(12 + std::cos(angle) * 9.0, 12 + std::sin(angle) * 9.0);
            painter.drawLine(inner, outer);
        }
        painter.drawEllipse(QPointF(12, 12), 8.4, 8.4);
        break;
    }
    case Glyph::Play: {
        QPainterPath path;
        path.moveTo(8, 5.5);
        path.lineTo(19, 12);
        path.lineTo(8, 18.5);
        path.closeSubpath();
        painter.setBrush(color);
        painter.setPen(Qt::NoPen);
        painter.drawPath(path);
        break;
    }
    case Glyph::Pause:
        painter.setBrush(color);
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(QRectF(7, 5, 3.4, 14), 1, 1);
        painter.drawRoundedRect(QRectF(13.6, 5, 3.4, 14), 1, 1);
        break;
    case Glyph::Reset:
        painter.drawArc(QRectF(4.5, 4.5, 15, 15), 35 * 16, 290 * 16);
        line(17, 4, 17, 8);
        line(17, 4, 13, 4);
        break;
    case Glyph::Stop:
        painter.setBrush(color);
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(QRectF(6, 6, 12, 12), 2, 2);
        break;
    case Glyph::Screen:
        painter.drawRoundedRect(QRectF(3, 4.5, 18, 12), 2, 2);
        line(9, 20, 15, 20);
        line(12, 16.5, 12, 20);
        break;
    case Glyph::Present:
        painter.drawRoundedRect(QRectF(3, 4.5, 18, 12), 2, 2);
        line(9, 20, 15, 20);
        line(12, 16.5, 12, 20);
        {
            QPainterPath path;
            path.moveTo(10, 8);
            path.lineTo(15, 10.5);
            path.lineTo(10, 13);
            path.closeSubpath();
            painter.setBrush(color);
            painter.setPen(Qt::NoPen);
            painter.drawPath(path);
        }
        break;
    case Glyph::Stage:
        painter.drawRoundedRect(QRectF(3, 4, 18, 12), 2, 2);
        line(7, 19, 17, 19);
        line(7, 16, 7, 19);
        line(17, 16, 17, 19);
        line(12, 16, 12, 19);
        painter.drawEllipse(QPointF(9.5, 8.5), 1.6, 1.6);
        painter.drawArc(QRectF(6.8, 9, 5.4, 4), 20 * 16, 140 * 16);
        line(15, 8, 15, 12);
        break;
    case Glyph::Timer:
        painter.drawEllipse(QPointF(12, 13), 7.3, 7.3);
        line(12, 13, 12, 9);
        line(12, 13, 15.2, 14.6);
        line(10, 3.5, 14, 3.5);
        line(12, 3.5, 12, 5.5);
        break;
    case Glyph::ChevronDown:
        painter.drawPolyline(QPolygonF({QPointF(7, 10), QPointF(12, 15), QPointF(17, 10)}));
        break;
    case Glyph::Trash:
        line(5, 7, 19, 7);
        line(9, 7, 9, 4.5);
        line(15, 7, 15, 4.5);
        painter.drawRoundedRect(QRectF(7, 7, 10, 12.5), 1.5, 1.5);
        line(10.5, 10.5, 10.5, 16.5);
        line(13.5, 10.5, 13.5, 16.5);
        break;
    case Glyph::Duplicate:
        painter.drawRoundedRect(QRectF(4, 4, 11, 12), 1.5, 1.5);
        painter.drawRoundedRect(QRectF(9, 9, 11, 11), 1.5, 1.5);
        break;
    case Glyph::Text:
        line(5, 6, 19, 6);
        line(12, 6, 12, 18.5);
        line(8.5, 18.5, 15.5, 18.5);
        break;
    case Glyph::Shape:
        painter.drawRoundedRect(QRectF(4, 5, 16, 14), 3, 3);
        break;
    case Glyph::Image:
        painter.drawRoundedRect(QRectF(3.5, 5, 17, 14), 2, 2);
        painter.drawEllipse(QPointF(8.5, 9.5), 1.4, 1.4);
        painter.drawPolyline(QPolygonF({QPointF(5.5, 17), QPointF(10, 12), QPointF(13, 15),
                                        QPointF(16, 11), QPointF(19.5, 15)}));
        break;
    case Glyph::Search:
        painter.drawEllipse(QPointF(10.5, 10.5), 6.2, 6.2);
        line(15, 15, 20, 20);
        break;
    case Glyph::Fit:
        line(3.5, 8, 3.5, 3.5);
        line(3.5, 3.5, 8, 3.5);
        line(16, 3.5, 20.5, 3.5);
        line(20.5, 3.5, 20.5, 8);
        line(20.5, 16, 20.5, 20.5);
        line(20.5, 20.5, 16, 20.5);
        line(8, 20.5, 3.5, 20.5);
        line(3.5, 20.5, 3.5, 16);
        break;
    case Glyph::ArrowLeft:
        painter.drawPolyline(QPolygonF({QPointF(14.5, 5.5), QPointF(8, 12), QPointF(14.5, 18.5)}));
        break;
    case Glyph::ArrowRight:
        painter.drawPolyline(QPolygonF({QPointF(9.5, 5.5), QPointF(16, 12), QPointF(9.5, 18.5)}));
        break;
    case Glyph::More:
        painter.setBrush(color);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(QPointF(6, 12), 1.4, 1.4);
        painter.drawEllipse(QPointF(12, 12), 1.4, 1.4);
        painter.drawEllipse(QPointF(18, 12), 1.4, 1.4);
        break;
    case Glyph::Folder: {
        QPainterPath path;
        path.moveTo(3.5, 7);
        path.lineTo(9, 7);
        path.lineTo(11, 9);
        path.lineTo(20.5, 9);
        path.lineTo(20.5, 18.5);
        path.lineTo(3.5, 18.5);
        path.closeSubpath();
        painter.drawPath(path);
        break;
    }
    case Glyph::Save:
        painter.drawRoundedRect(QRectF(4, 4, 16, 16), 2, 2);
        line(8, 4, 8, 9);
        line(16, 4, 16, 9);
        painter.drawRoundedRect(QRectF(7.5, 13, 9, 7), 1, 1);
        break;
    case Glyph::Close:
        line(6, 6, 18, 18);
        line(18, 6, 6, 18);
        break;
    case Glyph::Check:
        painter.drawPolyline(QPolygonF({QPointF(4.5, 12.5), QPointF(9.5, 17.5),
                                        QPointF(19.5, 6.5)}));
        break;
    case Glyph::Grid:
        painter.drawRoundedRect(QRectF(4, 4, 6.5, 6.5), 1, 1);
        painter.drawRoundedRect(QRectF(13.5, 4, 6.5, 6.5), 1, 1);
        painter.drawRoundedRect(QRectF(4, 13.5, 6.5, 6.5), 1, 1);
        painter.drawRoundedRect(QRectF(13.5, 13.5, 6.5, 6.5), 1, 1);
        break;
    case Glyph::Blackout:
        painter.setBrush(color);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(QPointF(12, 12), 7.5, 7.5);
        break;
    case Glyph::Logo:
        painter.drawRoundedRect(QRectF(4, 4, 16, 16), 3, 3);
        line(8, 15, 8, 9);
        line(8, 9, 12, 13);
        line(12, 13, 16, 9);
        line(16, 9, 16, 15);
        break;
    case Glyph::ZoomIn:
        painter.drawEllipse(QPointF(10.5, 10.5), 6.2, 6.2);
        line(15, 15, 20, 20);
        line(7.8, 10.5, 13.2, 10.5);
        line(10.5, 7.8, 10.5, 13.2);
        break;
    case Glyph::ZoomOut:
        painter.drawEllipse(QPointF(10.5, 10.5), 6.2, 6.2);
        line(15, 15, 20, 20);
        line(7.8, 10.5, 13.2, 10.5);
        break;
    }
    painter.restore();
}

QPixmap glyphPixmap(Glyph glyph, const QColor &color, int size)
{
    const int scale = 2;
    QPixmap pixmap(size * scale, size * scale);
    pixmap.setDevicePixelRatio(scale);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    paintGlyph(painter, glyph, QRectF(0, 0, size, size), color);
    return pixmap;
}

QIcon icon(Glyph glyph, const QColor &color, int size)
{
    return QIcon(glyphPixmap(glyph, color, size));
}

void paintAppMark(QPainter &painter, const QRectF &rect)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    const qreal side = qMin(rect.width(), rect.height());
    const QRectF box(rect.center().x() - side / 2.0, rect.center().y() - side / 2.0, side, side);

    QLinearGradient background(box.topLeft(), box.bottomRight());
    background.setColorAt(0.0, QColor(27, 45, 74));
    background.setColorAt(1.0, QColor(10, 15, 24));
    painter.setPen(Qt::NoPen);
    painter.setBrush(background);
    painter.drawRoundedRect(box, side * 0.22, side * 0.22);

    const qreal inset = side * 0.24;
    const qreal slideWidth = side * 0.40;
    const qreal slideHeight = side * 0.28;
    QPen frame(QColor(238, 242, 248, 225), qMax(1.2, side * 0.026));
    frame.setJoinStyle(Qt::RoundJoin);
    painter.setPen(frame);
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(QRectF(box.left() + inset, box.top() + side * 0.27, slideWidth,
                                   slideHeight),
                            side * 0.035, side * 0.035);
    painter.setPen(QPen(QColor(91, 141, 239), qMax(1.2, side * 0.026)));
    painter.drawRoundedRect(QRectF(box.left() + side * 0.36, box.top() + side * 0.43,
                                   slideWidth, slideHeight),
                            side * 0.035, side * 0.035);

    QPainterPath play;
    const QPointF center(box.center().x() + side * 0.14, box.center().y() + side * 0.02);
    play.moveTo(center + QPointF(-side * 0.055, -side * 0.075));
    play.lineTo(center + QPointF(side * 0.095, 0));
    play.lineTo(center + QPointF(-side * 0.055, side * 0.075));
    play.closeSubpath();
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(91, 141, 239));
    painter.drawPath(play);

    painter.restore();
}

QPixmap appMarkPixmap(int size)
{
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    paintAppMark(painter, QRectF(0, 0, size, size));
    return pixmap;
}

QIcon appIcon()
{
    QPixmap resource(QStringLiteral(":/icons/app-256.png"));
    if (!resource.isNull())
        return QIcon(resource);
    return QIcon(appMarkPixmap(256));
}

} // namespace mxu::ui
