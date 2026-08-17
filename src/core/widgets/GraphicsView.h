#pragma once

#include "core/CoreExport.h"

#include <QColor>
#include <QImage>
#include <QList>
#include <QPoint>
#include <QWidget>

namespace OFS {

// Workspace of the Sprites and Animations editors: images around an origin (the axis) with rulers,
// guides and crosses. Positions are in units (sprite pixels) from the origin.
class OFS_CORE_EXPORT GraphicsView : public QWidget
{
    Q_OBJECT

public:
    struct Image
    {
        QImage image;
        QPoint topLeft;
        double opacity = 1.0;
        bool draggable = false;
    };

    enum class Background
    {
        Color,
        Checker,
        Picture,
    };

    // What zooming keeps in place.
    enum class ZoomCenter
    {
        Axis,
        Workspace,
    };

    static QList<double> largeZoomRange(); // 0.25, 0.5, 1 ... 8
    static QList<double> smallZoomRange(); // 0.25, 0.5, 1 ... 4
    // Sky and grid over a floor. At offset (-426, -416) its floor line sits on the axis.
    static QImage defaultBackground();

    explicit GraphicsView(QWidget* parent = nullptr);

    void setImages(const QList<Image>& images);
    const QList<Image>& images() const { return m_images; }

    void setZoomLevels(const QList<double>& levels);
    const QList<double>& zoomLevels() const { return m_zoomLevels; }
    void setZoomIndex(int index);
    int zoomIndex() const { return m_zoomIndex; }
    double zoom() const;
    void zoomIn();
    void zoomOut();
    void zoomReset();
    void zoomFit();
    void setZoomCenter(ZoomCenter center) { m_zoomCenter = center; }
    ZoomCenter zoomCenter() const { return m_zoomCenter; }

    // Widget position.
    QPoint origin() const { return m_origin; }
    void setOrigin(const QPoint& origin);
    void centerOrigin();

    void setBackground(Background background)
    {
        m_background = background;
        update();
    }
    void setBackgroundColor(const QColor& color)
    {
        m_backgroundColor = color;
        update();
    }
    void setBackgroundPicture(const QImage& picture, const QPoint& offset);

    void setRulersVisible(bool visible)
    {
        m_rulers = visible;
        update();
    }
    bool rulersVisible() const { return m_rulers; }
    void setMainCrossVisible(bool visible)
    {
        m_mainCross = visible;
        update();
    }
    void setGuidesVisible(bool visible)
    {
        m_guides = visible;
        update();
    }
    bool guidesVisible() const { return m_guides; }
    QPoint guides() const { return m_guidePosition; }
    void setGuides(const QPoint& position);
    void setImagesOverGuides(bool over)
    {
        m_imagesOverGuides = over;
        update();
    }

    QPointF toUnits(const QPointF& widgetPoint) const;
    QPointF toWidget(const QPointF& units) const;

    QSize sizeHint() const override { return {640, 480}; }

Q_SIGNALS:
    void zoomChanged(int index);
    void originChanged(const QPoint& origin);
    void guidesChanged(const QPoint& position);
    void imageDragged(int dx, int dy);
    void dragFinished();
    void cursorMoved(const QPoint& units);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    enum class Drag
    {
        None,
        Pan,
        Image,
        VerticalGuide,
        HorizontalGuide,
    };
    void zoomTo(int index, const QPointF& anchor);
    QPointF zoomAnchor() const;
    void drawCross(QPainter& p);
    QRect contentRect() const;
    void drawBackground(QPainter& p, const QRect& area);
    void drawGuides(QPainter& p, const QRect& area);
    void drawImages(QPainter& p);
    void drawRulers(QPainter& p);
    Drag hitTest(const QPoint& pos) const;

    QList<Image> m_images;
    QList<double> m_zoomLevels;
    int m_zoomIndex = 0;
    ZoomCenter m_zoomCenter = ZoomCenter::Axis;
    QPoint m_origin{320, 360};
    Background m_background = Background::Color;
    QColor m_backgroundColor{64, 64, 64};
    QImage m_backgroundPicture;
    QPoint m_backgroundOffset;
    bool m_rulers = true;
    bool m_mainCross = true;
    bool m_guides = false;
    bool m_imagesOverGuides = false;
    QPoint m_guidePosition;
    Drag m_drag = Drag::None;
    QPoint m_dragStart;
    QPointF m_dragUnits;
    QPoint m_originAtStart;
};

} // namespace OFS
