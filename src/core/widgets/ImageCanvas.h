#pragma once

#include "core/CoreExport.h"
#include "core/sprites/ImageDocument.h"

#include <QImage>
#include <QWidget>

namespace OFS {

// Image editor canvas. Ctrl+drag pans, other presses are reported in image pixels.
class OFS_CORE_EXPORT ImageCanvas : public QWidget
{
    Q_OBJECT

public:
    explicit ImageCanvas(QWidget* parent = nullptr);

    void setDocument(const ImageDocument* document);
    void refresh();

    double zoom() const { return m_zoom; }
    double minimumZoom() const;
    static constexpr double MaximumZoom = 32.0;
    void setZoom(double zoom);
    void zoomIn();
    void zoomOut();
    void zoomReset();
    void zoomFit();
    void setPanning(bool panning) { m_panning = panning; }
    void setBrushSize(int size);
    void setOriginalShown(bool shown);
    bool originalShown() const { return m_original; }

    // pixelAt floors. gridPointAt rounds to the nearest grid point (for selections).
    QPoint pixelAt(const QPointF& point) const;
    QPoint gridPointAt(const QPointF& point) const;
    QPointF toWidget(const QPointF& imagePoint) const;

    QSize sizeHint() const override { return {640, 480}; }

Q_SIGNALS:
    void pressed(const QPoint& pixel, const QPoint& gridPoint, Qt::MouseButton button, Qt::KeyboardModifiers modifiers);
    void dragged(const QPoint& pixel, const QPoint& gridPoint);
    void released(const QPoint& pixel, const QPoint& gridPoint);
    void zoomChanged(double zoom);

protected:
    void paintEvent(QPaintEvent* event) override;
    void paintContent(QPainter& p);
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void leaveEvent(QEvent* event) override;

public:
    void setWorkspaceColors(const QColor& top, const QColor& bottom)
    {
        m_workspaceTop = top;
        m_workspaceBottom = bottom;
        update();
    }

private:
    QColor m_workspaceTop{58, 58, 58};
    QColor m_workspaceBottom{30, 30, 30};
    QWidget* m_surface = nullptr;
    QRectF canvasArea() const;
    QRectF editArea() const;
    void drawCanvas(QPainter& p, const QRectF& target) const;

    const ImageDocument* m_document = nullptr;
    QImage m_composite;
    QImage m_dimmed;
    double m_zoom = 1.0;
    QPointF m_center;
    bool m_panning = false;
    bool m_original = false;
    int m_brushSize = 0;
    QPoint m_hover{-1, -1};
    bool m_dragPan = false;
    bool m_pressed = false;
    QPointF m_panStart;
    QPointF m_centerAtStart;
};

} // namespace OFS
