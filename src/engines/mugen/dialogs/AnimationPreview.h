#pragma once

#include <QColor>
#include <QImage>
#include <QList>
#include <QPointF>
#include <QWidget>

namespace OFS::Mugen {

// Drawing area of the Throw creator. Positions are in screen units from the origin on the floor line.
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

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    QRect viewport() const;

    QList<Figure> m_figures;
    QSize m_screen{640, 480};
    // A left drag moves the view, same as Fighter Factory.
    QPointF m_pan;
    QPointF m_panAtStart;
    QPoint m_panStart;
    bool m_panning = false;
};

} // namespace OFS::Mugen
