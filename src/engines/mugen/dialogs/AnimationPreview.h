#pragma once

#include "core/widgets/WheelSteps.h"

#include <QColor>
#include <QImage>
#include <QList>
#include <QPointF>
#include <QWidget>

namespace OFS::Mugen {

// Drawing area of the Throw creator and the Offset viewer. Positions are in screen units from the origin on
// the floor line. The wheel zooms through the Sprites editor's levels and the origin stays where it is.
class AnimationPreview : public QWidget
{
    Q_OBJECT

public:
    struct Figure
    {
        QImage image;
        QPoint axis;
        QPointF position;
        double scale = 1.0; // screen units per sprite pixel
        bool flipH = false;
        bool flipV = false;
        QColor cross = Qt::white;
    };

    explicit AnimationPreview(QWidget* parent = nullptr);

    void setFigures(const QList<Figure>& figures);
    const QList<Figure>& figures() const { return m_figures; }
    // 640x480 by default. One screen unit is one pixel of the preview.
    void setScreenSize(const QSize& size);
    QSize screenSize() const { return m_screen; }
    QPointF origin() const;
    QPointF toWidget(const QPointF& units) const;
    // Widget pixels per screen unit.
    double zoom() const { return m_levels.value(m_zoomIndex, 1.0); }
    void zoomIn() { setZoomIndex(m_zoomIndex + 1); }
    void zoomOut() { setZoomIndex(m_zoomIndex - 1); }

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    QRect viewport() const;
    void setZoomIndex(int index);

    QList<Figure> m_figures;
    QSize m_screen{640, 480};
    QList<double> m_levels;
    int m_zoomIndex = 0;
    WheelSteps m_wheel;
    // A left drag moves the view, same as Fighter Factory.
    QPointF m_pan;
    QPointF m_panAtStart;
    QPoint m_panStart;
    bool m_panning = false;
};

} // namespace OFS::Mugen
