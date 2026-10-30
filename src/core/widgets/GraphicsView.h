#pragma once

#include "core/CoreExport.h"
#include "core/widgets/WheelSteps.h"

#include <QColor>
#include <QImage>
#include <QList>
#include <QPainter>
#include <QPoint>
#include <QRectF>
#include <QTransform>
#include <QWidget>

#include <algorithm>

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
        QTransform transform;
        QRectF clip;
        QPainter::CompositionMode mode = QPainter::CompositionMode_SourceOver;
    };

    struct Frame
    {
        QRectF rect;
        QColor color;
        bool dashed = false;
        bool handles = false;
    };

    struct Box
    {
        QRect rect;
        QColor color;
        bool selected = false;
        bool editable = true;
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
    // With the Color picker on, draggable images show a cross over their whole rectangle and a
    // press doesn't drag them.
    void setColorPicker(bool on);
    bool colorPicker() const { return m_colorPicker; }
    void setBoxes(const QList<Box>& boxes);
    const QList<Box>& boxes() const { return m_boxes; }
    void setDrawingBoxes(bool drawing, const QColor& color = Qt::red);
    bool drawingBoxes() const { return m_drawingBoxes; }
    void setFrames(const QList<Frame>& frames);
    const QList<Frame>& frames() const { return m_frames; }
    void setMainCrossPosition(const QPoint& position);
    QPoint mainCrossPosition() const { return m_mainCrossPosition; }
    // The secondary cross shows the frame offset. The extra cross sits on the origin.
    void setSecondaryCross(bool visible, const QPoint& position);
    bool secondaryCrossVisible() const { return m_secondaryCross; }
    QPoint secondaryCrossPosition() const { return m_secondaryPosition; }
    void setExtraCrossVisible(bool visible);
    bool extraCrossVisible() const { return m_extraCross; }

    void setZoomLevels(const QList<double>& levels);
    const QList<double>& zoomLevels() const { return m_zoomLevels; }
    void setZoomIndex(int index);
    int zoomIndex() const { return m_zoomIndex; }
    double zoom() const;
    void zoomIn();
    void zoomOut();
    void zoomReset();
    void zoomFit();
    void setFitArea(const QRectF& area) { m_fitArea = area; }
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
    Background background() const { return m_background; }
    void setBackgroundColor(const QColor& color) { setBackgroundGradient(color, color); }
    void setBackgroundGradient(const QColor& top, const QColor& bottom)
    {
        m_backgroundColor = top;
        m_backgroundEnd = bottom;
        update();
    }
    QColor backgroundColor() const { return m_backgroundColor; }
    QColor backgroundEnd() const { return m_backgroundEnd; }
    void setBackgroundPicture(const QImage& picture, const QPoint& offset);
    const QImage& backgroundPicture() const { return m_backgroundPicture; }
    QPoint backgroundOffset() const { return m_backgroundOffset; }

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
    bool mainCrossVisible() const { return m_mainCross; }
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
    bool imagesOverGuides() const { return m_imagesOverGuides; }
    enum class CrossStyle
    {
        Default,
        Modern,
        Custom,
    };
    void setCrossStyle(CrossStyle style, const QImage& main = {}, const QImage& secondary = {},
                       const QImage& extra = {});
    CrossStyle crossStyle() const { return m_crossStyle; }
    const QImage& crossPicture(int which) const { return m_crossImages[std::clamp(which, 0, 2)]; }
    void setBoxOpacity(double opacity);
    double boxOpacity() const { return m_boxOpacity; }
    void applyGraphicsSettings(const class Config& config);
    // Onion skin effect: 0 none, 1 monochrome, 2 lighter, 3 darker.
    static QImage onionEffect(const QImage& image, int effect);

    void setBoxesOverCrosses(bool over)
    {
        m_boxesOverCrosses = over;
        update();
    }
    bool boxesOverCrosses() const { return m_boxesOverCrosses; }

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
    void boxPressed(int index);
    void imagePressed(int index);
    void boxEdited(int index, const QRect& rect);
    void boxDrawn(const QRect& rect);
    void boxNudged(int dx, int dy);
    void boxDeleteRequested();
    void rightClicked();
    void pressed(const QPointF& units);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;

private:
    WheelSteps m_wheel;
    static QImage makeDefaultBackground();
    enum class Drag
    {
        None,
        Pan,
        Image,
        VerticalGuide,
        HorizontalGuide,
        BoxMove,
        BoxResize,
        BoxDraw,
        Pick, // an image under the Color picker, never dragged
    };
    // The mouse pointer for what is under pos, or for the drag going on.
    void updatePointer(const QPoint& pos, Qt::KeyboardModifiers modifiers);
    static Qt::CursorShape pointerFor(Drag drag, int handle);
    void zoomTo(int index, const QPointF& anchor);
    QPointF zoomAnchor() const;
    void drawCross(QPainter& p, const QPointF& at, const QColor& fill, const QColor& outline, int which = 0);
    void drawBoxes(QPainter& p);
    QRectF boxRect(const QRect& units) const;
    QList<QRectF> handles(const QRect& units) const;
    int selectedBox() const;
    QRect contentRect() const;
    void paintContent(QPainter& p);
    bool needsRasterImages(const QPainter& p) const;
    void drawBackground(QPainter& p, const QRect& area);
    void drawGuides(QPainter& p, const QRect& area);
    void drawImages(QPainter& p);
    void drawRulers(QPainter& p);
    void fillWorkspace(QPainter& p, const QRect& area) const;
    Drag hitTest(const QPoint& pos, int* box = nullptr, int* handle = nullptr, int* imageIndex = nullptr) const;
    void drawFrames(QPainter& p);

    QList<Image> m_images;
    QList<double> m_zoomLevels;
    int m_zoomIndex = 0;
    ZoomCenter m_zoomCenter = ZoomCenter::Axis;
    QPoint m_origin{320, 360};
    Background m_background = Background::Color;
    QColor m_backgroundColor{64, 64, 64};
    QRectF m_fitArea;
    QColor m_backgroundEnd{64, 64, 64};
    QImage m_backgroundPicture;
    QPoint m_backgroundOffset;
    bool m_rulers = true;
    bool m_mainCross = true;
    QPoint m_mainCrossPosition;
    QList<Frame> m_frames;
    bool m_guides = false;
    bool m_imagesOverGuides = false;
    bool m_boxesOverCrosses = false;
    CrossStyle m_crossStyle = CrossStyle::Default;
    QImage m_crossImages[3];
    double m_boxOpacity = 0.5;
    QWidget* m_surface = nullptr;
    QPoint m_guidePosition;
    Drag m_drag = Drag::None;
    QPoint m_dragStart;
    QPointF m_dragUnits;
    QPoint m_originAtStart;
    QList<Box> m_boxes;
    bool m_drawingBoxes = false;
    QColor m_drawingColor;
    bool m_secondaryCross = false;
    QPoint m_secondaryPosition;
    bool m_extraCross = false;
    int m_dragBox = -1;
    int m_dragHandle = -1; // 0 top-left, clockwise to 7 left
    QRect m_dragBoxStart;
    QRect m_drawnBox;
    bool m_ctrlPan = false; // a Ctrl+drag pans with the hand kept open
    bool m_colorPicker = false;
};

} // namespace OFS
