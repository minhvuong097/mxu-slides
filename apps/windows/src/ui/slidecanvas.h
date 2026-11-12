#pragma once

#include <QPointF>
#include <QRectF>
#include <QWidget>

namespace mxu {
class Editor;
class SlideObject;
class SlideRenderer;
}

namespace mxu::ui {

class SlideCanvas : public QWidget {
    Q_OBJECT
public:
    explicit SlideCanvas(Editor *editor, SlideRenderer *renderer, QWidget *parent = nullptr);

    void refresh();
    void fitToWindow();
    void zoomIn();
    void zoomOut();
    int zoomPercent() const;
    void addTextObject() { addText(); }
    void addShapeObject() { addShape(); }
    void addImageObject() { addImage(); }

signals:
    void zoomChanged(int percent);
    void textEditingRequested(const QString &objectId);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    enum class Handle { None, TopLeft, Top, TopRight, Right, BottomRight, Bottom, BottomLeft, Left };
    enum class Interaction { None, Move, Resize, Pan };

    void drawEmptyState(QPainter &painter);
    QRectF computeSlideRect() const;
    QPointF toSlide(const QPointF &point) const;
    QPointF toView(const QPointF &point) const;
    QRectF toView(const QRectF &rect) const;
    QString objectAt(const QPointF &viewPoint) const;
    QRectF selectedViewRect() const;
    Handle handleAt(const QPointF &viewPoint) const;
    QRectF handleRect(Handle handle) const;
    void updateHover(const QPointF &viewPoint);
    void updateCursor(const QPointF &viewPoint);
    void addText();
    void addShape();
    void addImage();
    void nudgeSelected(double dx, double dy);
    void applyFrame(const QRectF &frame, bool resize, Handle handle);
    QCursor cursorForHandle(Handle handle) const;
    qreal fitScale() const;
    void emitZoom();

    Editor *m_editor = nullptr;
    SlideRenderer *m_renderer = nullptr;

    qreal m_zoom = 1.0;
    qreal m_scale = 1.0;
    QRectF m_slideRect;
    QPointF m_panOffset;
    int m_lastZoomPercent = -1;

    Interaction m_interaction = Interaction::None;
    Handle m_activeHandle = Handle::None;
    QString m_draggedId;
    QString m_hoverId;
    QPointF m_pressView;
    QPointF m_pressSlide;
    QRectF m_pressFrame;
    QPointF m_panStart;
    bool m_dragMoved = false;
};

} // namespace mxu::ui
