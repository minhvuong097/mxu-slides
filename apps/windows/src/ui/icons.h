#pragma once

#include <QColor>
#include <QIcon>
#include <QPixmap>

class QPainter;
class QRectF;

namespace mxu::ui {

enum class Glyph {
    Plus,
    Import,
    Export,
    Undo,
    Redo,
    Settings,
    Play,
    Pause,
    Reset,
    Stop,
    Screen,
    Stage,
    Timer,
    ChevronDown,
    Trash,
    Duplicate,
    Text,
    Shape,
    Image,
    Search,
    Fit,
    ArrowLeft,
    ArrowRight,
    More,
    Folder,
    Save,
    Close,
    Check,
    Grid,
    Present,
    Blackout,
    Logo,
    ZoomIn,
    ZoomOut
};

QIcon icon(Glyph glyph, const QColor &color, int size = 18);
QPixmap glyphPixmap(Glyph glyph, const QColor &color, int size = 18);
void paintGlyph(QPainter &painter, Glyph glyph, const QRectF &rect, const QColor &color,
                qreal strokeWidth = 1.8);

QPixmap appMarkPixmap(int size);
void paintAppMark(QPainter &painter, const QRectF &rect);
QIcon appIcon();

} // namespace mxu::ui
