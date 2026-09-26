#include "core/widgets/GraphicsView.h"

#include "core/settings/Config.h"
#include "core/widgets/Checkerboard.h"
#include "core/widgets/RenderSurface.h"
#include "core/widgets/Rulers.h"

#include <QDateTime>
#include <QFileInfo>
#include <QHash>
#include <QKeyEvent>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPaintEngine>
#include <QPainter>
#include <QWheelEvent>

#include <cmath>

namespace OFS {

namespace {

constexpr int RulerSize = 20;
constexpr int GuideGrab = 3;
constexpr int HandleSize = 7;

} // namespace

QList<double> GraphicsView::largeZoomRange()
{
    return {0.25, 0.5, 1, 2, 3, 4, 5, 6, 7, 8};
}

QList<double> GraphicsView::smallZoomRange()
{
    return {0.25, 0.5, 1, 2, 3, 4};
}

QImage GraphicsView::defaultBackground()
{
    static const QImage backdrop = makeDefaultBackground();
    return backdrop;
}

QImage GraphicsView::makeDefaultBackground()
{
    // 852 x 560: sky down to the floor line at 416, then floor. Grid lines every 32.
    QImage image(852, 560, QImage::Format_RGB32);
    QPainter p(&image);
    QLinearGradient sky(0, 0, 0, 416);
    sky.setColorAt(0, QColor(40, 58, 88));
    sky.setColorAt(1, QColor(76, 108, 148));
    p.fillRect(0, 0, 852, 416, sky);
    QLinearGradient floor(0, 416, 0, 560);
    floor.setColorAt(0, QColor(70, 98, 136));
    floor.setColorAt(1, QColor(104, 142, 186));
    p.fillRect(0, 416, 852, 144, floor);
    p.setPen(QColor(140, 176, 220, 110));
    for (int x = 10; x < 852; x += 32)
        p.drawLine(x, 0, x, 559);
    for (int y = 416 % 32; y < 560; y += 32)
        p.drawLine(0, y, 851, y);
    p.setPen(QColor(200, 222, 245));
    p.drawLine(0, 416, 851, 416);
    return image;
}

GraphicsView::GraphicsView(QWidget* parent)
    : QWidget(parent)
    , m_zoomLevels(largeZoomRange())
    , m_zoomIndex(2)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_surface = Rendering::attachSurface(this, [this](QPainter& p) { paintContent(p); });
}

void GraphicsView::setImages(const QList<Image>& images)
{
    m_images = images;
    update();
}

void GraphicsView::setBoxes(const QList<Box>& boxes)
{
    m_boxes = boxes;
    if (m_drag == Drag::BoxMove || m_drag == Drag::BoxResize)
        m_drag = Drag::None;
    update();
}

void GraphicsView::setDrawingBoxes(bool drawing, const QColor& color)
{
    m_drawingBoxes = drawing;
    m_drawingColor = color;
    if (!drawing && m_drag == Drag::BoxDraw)
        m_drag = Drag::None;
    update();
}

void GraphicsView::setSecondaryCross(bool visible, const QPoint& position)
{
    m_secondaryCross = visible;
    m_secondaryPosition = position;
    update();
}

void GraphicsView::setExtraCrossVisible(bool visible)
{
    m_extraCross = visible;
    update();
}

int GraphicsView::selectedBox() const
{
    for (int i = 0; i < m_boxes.size(); ++i) {
        if (m_boxes.at(i).selected && m_boxes.at(i).editable)
            return i;
    }
    return -1;
}

QRectF GraphicsView::boxRect(const QRect& units) const
{
    const QRect r = units.normalized();
    return QRectF(toWidget(QPointF(r.left(), r.top())), toWidget(QPointF(r.right(), r.bottom()))).normalized();
}

QList<QRectF> GraphicsView::handles(const QRect& units) const
{
    const QRectF r = boxRect(units);
    const QPointF points[] = {
        r.topLeft(),     QPointF(r.center().x(), r.top()),    r.topRight(),   QPointF(r.right(), r.center().y()),
        r.bottomRight(), QPointF(r.center().x(), r.bottom()), r.bottomLeft(), QPointF(r.left(), r.center().y())};
    QList<QRectF> result;
    for (const QPointF& at : points)
        result << QRectF(at - QPointF(HandleSize / 2.0, HandleSize / 2.0), QSizeF(HandleSize, HandleSize));
    return result;
}

void GraphicsView::setZoomLevels(const QList<double>& levels)
{
    if (levels.isEmpty())
        return;
    const double current = zoom();
    m_zoomLevels = levels;
    int best = 0;
    for (int i = 0; i < m_zoomLevels.size(); ++i) {
        if (std::abs(m_zoomLevels[i] - current) < std::abs(m_zoomLevels[best] - current))
            best = i;
    }
    m_zoomIndex = best;
    update();
}

double GraphicsView::zoom() const
{
    return m_zoomLevels.value(m_zoomIndex, 1.0);
}

void GraphicsView::setZoomIndex(int index)
{
    zoomTo(index, QPointF(m_origin));
}

void GraphicsView::zoomTo(int index, const QPointF& anchor)
{
    index = std::clamp(index, 0, int(m_zoomLevels.size()) - 1);
    if (index == m_zoomIndex)
        return;
    const QPointF units = toUnits(anchor);
    m_zoomIndex = index;
    const QPointF moved = toWidget(units);
    setOrigin(m_origin + (anchor - moved).toPoint());
    update();
    Q_EMIT zoomChanged(m_zoomIndex);
}

QPointF GraphicsView::zoomAnchor() const
{
    return m_zoomCenter == ZoomCenter::Axis ? QPointF(m_origin) : QPointF(contentRect().center());
}

void GraphicsView::zoomIn()
{
    zoomTo(m_zoomIndex + 1, zoomAnchor());
}

void GraphicsView::zoomOut()
{
    zoomTo(m_zoomIndex - 1, zoomAnchor());
}

void GraphicsView::zoomReset()
{
    int best = 0;
    for (int i = 0; i < m_zoomLevels.size(); ++i) {
        if (std::abs(m_zoomLevels[i] - 1.0) < std::abs(m_zoomLevels[best] - 1.0))
            best = i;
    }
    zoomTo(best, zoomAnchor());
}

void GraphicsView::zoomFit()
{
    QRectF bounds = m_fitArea;
    if (bounds.isEmpty()) {
        for (const Image& image : std::as_const(m_images))
            bounds |= QRectF(image.transform.mapRect(QRect(image.topLeft, image.image.size())));
    }
    if (bounds.isEmpty())
        return;
    const QRect area = contentRect().adjusted(8, 8, -8, -8);
    int best = 0;
    for (int i = 0; i < m_zoomLevels.size(); ++i) {
        if (bounds.width() * m_zoomLevels[i] <= area.width() && bounds.height() * m_zoomLevels[i] <= area.height())
            best = i;
    }
    m_zoomIndex = best;
    const QPointF centre = bounds.center() * zoom();
    setOrigin((QPointF(area.center()) - centre).toPoint());
    update();
    Q_EMIT zoomChanged(m_zoomIndex);
}

void GraphicsView::setOrigin(const QPoint& origin)
{
    if (origin == m_origin)
        return;
    m_origin = origin;
    update();
    Q_EMIT originChanged(m_origin);
}

void GraphicsView::centerOrigin()
{
    // Middle of the whole widget, rulers included. Same as Fighter Factory.
    setOrigin(rect().center());
}

void GraphicsView::setBackgroundPicture(const QImage& picture, const QPoint& offset)
{
    m_backgroundPicture = picture;
    m_backgroundOffset = offset;
    update();
}

void GraphicsView::setGuides(const QPoint& position)
{
    if (position == m_guidePosition)
        return;
    m_guidePosition = position;
    update();
    Q_EMIT guidesChanged(m_guidePosition);
}

QPointF GraphicsView::toUnits(const QPointF& widgetPoint) const
{
    return (widgetPoint - QPointF(m_origin)) / zoom();
}

QPointF GraphicsView::toWidget(const QPointF& units) const
{
    return QPointF(m_origin) + units * zoom();
}

QRect GraphicsView::contentRect() const
{
    return m_rulers ? rect().adjusted(RulerSize, RulerSize, 0, 0) : rect();
}

void GraphicsView::drawBackground(QPainter& p, const QRect& area)
{
    switch (m_background) {
    case Background::Color:
        fillWorkspace(p, area);
        break;
    case Background::Checker:
        p.save();
        p.setBrushOrigin(area.topLeft());
        p.fillRect(area, checkerboard(8, QColor(240, 240, 240), QColor(204, 204, 204)));
        p.restore();
        break;
    case Background::Picture:
        fillWorkspace(p, area);
        if (!m_backgroundPicture.isNull()) {
            const QPointF at = toWidget(m_backgroundOffset);
            p.drawImage(QRectF(at, QSizeF(m_backgroundPicture.size()) * zoom()), m_backgroundPicture);
        }
        break;
    }
}

void GraphicsView::fillWorkspace(QPainter& p, const QRect& area) const
{
    if (m_backgroundColor == m_backgroundEnd) {
        p.fillRect(area, m_backgroundColor);
        return;
    }
    QLinearGradient gradient(area.topLeft(), area.bottomLeft());
    gradient.setColorAt(0, m_backgroundColor);
    gradient.setColorAt(1, m_backgroundEnd);
    p.fillRect(area, gradient);
}

void GraphicsView::drawGuides(QPainter& p, const QRect& area)
{
    if (!m_guides)
        return;
    const QPointF at = toWidget(m_guidePosition);
    const double thickness = std::max(1.0, zoom());
    p.fillRect(QRectF(at.x(), area.top(), thickness, area.height()), QColor(0, 255, 255));
    p.fillRect(QRectF(area.left(), at.y(), area.width(), thickness), QColor(0, 255, 255));
}

void GraphicsView::setCrossStyle(CrossStyle style, const QImage& main, const QImage& secondary, const QImage& extra)
{
    m_crossStyle = style;
    m_crossImages[0] = main;
    m_crossImages[1] = secondary;
    m_crossImages[2] = extra;
    update();
}

void GraphicsView::setBoxOpacity(double opacity)
{
    m_boxOpacity = std::clamp(opacity, 0.0, 1.0);
    update();
}

namespace {

// Settings picture (background, crosshair), reloaded only when it changes.
QImage pictureFile(const QString& path)
{
    if (path.isEmpty())
        return {};
    static QHash<QString, QPair<QDateTime, QImage>> cache;
    const QDateTime modified = QFileInfo(path).lastModified();
    auto found = cache.constFind(path);
    if (found != cache.cend() && found->first == modified)
        return found->second;
    const QImage image(path);
    cache.insert(path, {modified, image});
    return image;
}

} // namespace

void GraphicsView::applyGraphicsSettings(const Config& config)
{
    setZoomLevels(config.intValue(QStringLiteral("zoom_mode"), 1) == 0 ? smallZoomRange() : largeZoomRange());
    setZoomCenter(config.intValue(QStringLiteral("zoom_center"), 0) == 1 ? ZoomCenter::Workspace : ZoomCenter::Axis);
    const QPoint offset(config.intValue(QStringLiteral("background_x"), -426),
                        config.intValue(QStringLiteral("background_y"), -416));
    switch (config.intValue(QStringLiteral("background_type"), 1)) {
    case 0:
        setBackground(Background::Color);
        break;
    case 2:
        setBackgroundPicture(pictureFile(config.value(QStringLiteral("background_file"))), offset);
        setBackground(Background::Picture);
        break;
    default:
        setBackgroundPicture(defaultBackground(), offset);
        setBackground(Background::Picture);
        break;
    }
    setRulersVisible(config.boolValue(QStringLiteral("show_rulers"), true));
    setMainCrossVisible(config.boolValue(QStringLiteral("show_cross1"), true));
    setGuidesVisible(config.boolValue(QStringLiteral("show_guides"), false));
    setImagesOverGuides(config.boolValue(QStringLiteral("sprite_over_guides"), false));
    const int type = config.intValue(QStringLiteral("crosshair_type"), 0);
    setCrossStyle(type == 1   ? CrossStyle::Modern
                  : type == 2 ? CrossStyle::Custom
                              : CrossStyle::Default,
                  pictureFile(config.value(QStringLiteral("crosshair_main"))),
                  pictureFile(config.value(QStringLiteral("crosshair_secondary"))),
                  pictureFile(config.value(QStringLiteral("crosshair_extra"))));
    setBoxOpacity(config.value(QStringLiteral("box_opacity"), QStringLiteral("0.5")).toDouble());
}

QImage GraphicsView::onionEffect(const QImage& image, int effect)
{
    if (effect <= 0 || image.isNull())
        return image;
    QImage out = image.convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < out.height(); ++y) {
        auto* line = reinterpret_cast<QRgb*>(out.scanLine(y));
        for (int x = 0; x < out.width(); ++x) {
            const QRgb c = line[x];
            const int a = qAlpha(c);
            if (a == 0)
                continue;
            int r = qRed(c);
            int g = qGreen(c);
            int b = qBlue(c);
            if (effect == 1) {
                r = g = b = qGray(c);
            } else if (effect == 2) {
                r = (r + 255) / 2;
                g = (g + 255) / 2;
                b = (b + 255) / 2;
            } else {
                r /= 2;
                g /= 2;
                b /= 2;
            }
            line[x] = qRgba(r, g, b, a);
        }
    }
    return out;
}

void GraphicsView::drawCross(QPainter& p, const QPointF& at, const QColor& fill, const QColor& outlineColor, int which)
{
    if (m_crossStyle == CrossStyle::Custom && !m_crossImages[which].isNull()) {
        const QImage& picture = m_crossImages[which];
        const QPointF centre = toWidget(at);
        p.drawImage(
            QPointF(std::round(centre.x() - picture.width() / 2.0), std::round(centre.y() - picture.height() / 2.0)),
            picture);
        return;
    }
    if (m_crossStyle == CrossStyle::Modern) {
        const QPointF c = toWidget(at) + QPointF(zoom() / 2, zoom() / 2);
        const double arm = 12;
        const double gap = 3;
        QColor line = outlineColor;
        line.setAlpha(170);
        p.save();
        p.setRenderHint(QPainter::Antialiasing, false);
        p.setPen(QPen(line, 1));
        p.drawLine(QPointF(c.x() - arm, c.y()), QPointF(c.x() - gap, c.y()));
        p.drawLine(QPointF(c.x() + gap, c.y()), QPointF(c.x() + arm, c.y()));
        p.drawLine(QPointF(c.x(), c.y() - arm), QPointF(c.x(), c.y() - gap));
        p.drawLine(QPointF(c.x(), c.y() + gap), QPointF(c.x(), c.y() + arm));
        QColor dot = fill;
        dot.setAlpha(200);
        p.fillRect(QRectF(c.x() - 0.5, c.y() - 0.5, 1, 1), dot);
        p.restore();
        return;
    }
    // Default cross: a 13 x 13 plus with a hollow centre and an outline, scaled with the zoom.
    auto cells = [this, at](double x, double y, double w, double h) {
        return QRectF(toWidget(QPointF(at.x() + x, at.y() + y)), QSizeF(w, h) * zoom());
    };
    for (const QRectF& part : {cells(-7, -1, 6, 3), cells(2, -1, 6, 3), cells(-1, -7, 3, 6), cells(-1, 2, 3, 6),
                               cells(-1, -1, 3, 1), cells(-1, 1, 3, 1), cells(-1, 0, 1, 1), cells(1, 0, 1, 1)})
        p.fillRect(part, outlineColor);
    for (const QRectF& arm : {cells(-6, 0, 6, 1), cells(1, 0, 6, 1), cells(0, -6, 1, 6), cells(0, 1, 1, 6)})
        p.fillRect(arm, fill);
}

void GraphicsView::drawBoxes(QPainter& p)
{
    const double thickness = std::max(1.0, zoom());
    auto draw = [&](const QRect& units, const QColor& color, bool selected) {
        const QRectF r = boxRect(units);
        QColor fill = color;
        fill.setAlpha(int(std::lround(255 * m_boxOpacity)));
        p.fillRect(r, fill);
        if (selected)
            p.fillRect(r, QBrush(color, Qt::BDiagPattern));
        p.setPen(QPen(color, thickness, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
        p.setBrush(Qt::NoBrush);
        p.drawRect(r.adjusted(thickness / 2, thickness / 2, -thickness / 2, -thickness / 2));
    };
    for (const Box& box : std::as_const(m_boxes))
        draw(box.rect, box.color, box.selected);
    if (m_drag == Drag::BoxDraw && m_drawnBox.isValid())
        draw(m_drawnBox, m_drawingColor, false);
    const int selected = selectedBox();
    if (selected >= 0) {
        p.setPen(QColor(128, 128, 128));
        p.setBrush(Qt::white);
        for (const QRectF& handle : handles(m_boxes.at(selected).rect))
            p.drawRect(handle);
        p.setBrush(Qt::NoBrush);
    }
}

void GraphicsView::drawImages(QPainter& p)
{
    p.setRenderHint(QPainter::SmoothPixmapTransform, false);
    QTransform view;
    view.translate(m_origin.x(), m_origin.y());
    view.scale(zoom(), zoom());
    for (const Image& image : std::as_const(m_images)) {
        if (image.image.isNull())
            continue;
        p.save();
        if (!image.clip.isEmpty())
            p.setClipRect(QRectF(toWidget(image.clip.topLeft()), toWidget(image.clip.bottomRight())),
                          Qt::IntersectClip);
        p.setCompositionMode(image.mode);
        p.setOpacity(image.opacity);
        p.setTransform(image.transform * view, true);
        p.drawImage(QRectF(image.topLeft, QSizeF(image.image.size())), image.image);
        p.restore();
    }
    p.setOpacity(1.0);
}

void GraphicsView::drawRulers(QPainter& p)
{
    if (!m_rulers)
        return;
    int minor = 10;
    int major = 50;
    Rulers::steps(zoom(), &minor, &major);
    const QPointF zero = toWidget(QPointF(0, 0));
    const QPointF one = toWidget(QPointF(1, 1));
    Rulers::draw(p, rect(), contentRect(), {zero.x(), one.x() - zero.x()}, {zero.y(), one.y() - zero.y()}, minor, major,
                 Rulers::Colours::from(palette()), {RulerSize});
}

void GraphicsView::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    if (m_surface) {
        Rendering::update(m_surface);
        return;
    }
    QPainter p(this);
    paintContent(p);
}

bool GraphicsView::needsRasterImages(const QPainter& p) const
{
    // The OpenGL painter only has the Porter-Duff modes and Plus. Others like Difference (subtractive
    // blending) need an extension some drivers lack, so those images are composed in memory.
    if (!p.paintEngine() || p.paintEngine()->type() != QPaintEngine::OpenGL2)
        return false;
    return std::any_of(m_images.cbegin(), m_images.cend(), [](const Image& image) {
        return image.mode > QPainter::CompositionMode_Plus && image.mode <= QPainter::CompositionMode_Exclusion;
    });
}

void GraphicsView::paintContent(QPainter& p)
{
    const QRect area = contentRect();
    p.save();
    p.setClipRect(area);
    // Back to front: background, images, boxes, crosses, guides. The two "over" options swap the
    // guides with the images and the boxes with the crosses.
    if (needsRasterImages(p)) {
        QImage layers(size() * devicePixelRatioF(), QImage::Format_ARGB32_Premultiplied);
        layers.setDevicePixelRatio(devicePixelRatioF());
        layers.fill(Qt::transparent);
        {
            QPainter raster(&layers);
            raster.setClipRect(area);
            drawBackground(raster, area);
            if (m_imagesOverGuides)
                drawGuides(raster, area);
            drawImages(raster);
        }
        p.drawImage(QPoint(0, 0), layers);
    } else {
        drawBackground(p, area);
        if (m_imagesOverGuides)
            drawGuides(p, area);
        drawImages(p);
    }
    if (!m_boxesOverCrosses) {
        drawBoxes(p);
        drawFrames(p);
    }
    if (m_mainCross)
        drawCross(p, QPointF(m_mainCrossPosition), Qt::black, Qt::white, 0);
    if (m_extraCross)
        drawCross(p, QPointF(0, 0), Qt::white, QColor(0, 160, 0), 2);
    if (m_secondaryCross)
        drawCross(p, QPointF(m_secondaryPosition), Qt::white, QColor(0, 0, 255), 1);
    if (m_boxesOverCrosses) {
        drawBoxes(p);
        drawFrames(p);
    }
    if (!m_imagesOverGuides)
        drawGuides(p, area);
    p.restore();
    drawRulers(p);
}

void GraphicsView::setFrames(const QList<Frame>& frames)
{
    m_frames = frames;
    update();
}

void GraphicsView::setMainCrossPosition(const QPoint& position)
{
    m_mainCrossPosition = position;
    update();
}

void GraphicsView::drawFrames(QPainter& p)
{
    for (const Frame& frame : std::as_const(m_frames)) {
        const QRectF r(toWidget(frame.rect.topLeft()), toWidget(frame.rect.bottomRight()));
        QPen pen(frame.color, 1);
        if (frame.dashed)
            pen.setStyle(Qt::DashLine);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawRect(r);
        if (!frame.handles)
            continue;
        p.setPen(QPen(Qt::black, 1));
        p.setBrush(frame.color);
        const double s = 3;
        const QList<QPointF> points = {
            r.topLeft(),     QPointF(r.center().x(), r.top()),    r.topRight(),   QPointF(r.right(), r.center().y()),
            r.bottomRight(), QPointF(r.center().x(), r.bottom()), r.bottomLeft(), QPointF(r.left(), r.center().y())};
        for (const QPointF& at : points)
            p.drawRect(QRectF(at - QPointF(s, s), QSizeF(2 * s, 2 * s)));
        p.setBrush(Qt::NoBrush);
    }
}

GraphicsView::Drag GraphicsView::hitTest(const QPoint& pos, int* box, int* handle, int* imageIndex) const
{
    if (m_drawingBoxes)
        return Drag::BoxDraw;
    // Hit order: selected box handles, editable boxes from the top, then the rest.
    if (const int selected = selectedBox(); selected >= 0) {
        const QList<QRectF> grips = handles(m_boxes.at(selected).rect);
        for (int h = 0; h < grips.size(); ++h) {
            if (grips.at(h).contains(pos)) {
                if (box)
                    *box = selected;
                if (handle)
                    *handle = h;
                return Drag::BoxResize;
            }
        }
    }
    for (qsizetype i = m_boxes.size() - 1; i >= 0; --i) {
        if (m_boxes.at(i).editable && boxRect(m_boxes.at(i).rect).contains(pos)) {
            if (box)
                *box = int(i);
            return Drag::BoxMove;
        }
    }
    if (m_guides) {
        const QPointF at = toWidget(m_guidePosition);
        const double thickness = std::max(1.0, zoom());
        if (pos.x() >= at.x() - GuideGrab && pos.x() <= at.x() + thickness + GuideGrab)
            return Drag::VerticalGuide;
        if (pos.y() >= at.y() - GuideGrab && pos.y() <= at.y() + thickness + GuideGrab)
            return Drag::HorizontalGuide;
    }
    const QPointF units = toUnits(pos);
    for (qsizetype i = m_images.size() - 1; i >= 0; --i) {
        const Image& image = m_images[i];
        if (!image.draggable || image.image.isNull())
            continue;
        const QPointF mapped = image.transform.inverted().map(units) - QPointF(image.topLeft);
        const QPoint local(int(std::floor(mapped.x())), int(std::floor(mapped.y())));
        if (QRect(QPoint(), image.image.size()).contains(local) && image.image.pixelColor(local).alpha() > 0) {
            if (imageIndex)
                *imageIndex = int(i);
            return Drag::Image;
        }
    }
    return Drag::Pan;
}

void GraphicsView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::RightButton) {
        Q_EMIT rightClicked();
        return;
    }
    if (event->button() != Qt::LeftButton || !contentRect().contains(event->position().toPoint())) {
        QWidget::mousePressEvent(event);
        return;
    }
    setFocus(Qt::MouseFocusReason);
    Q_EMIT pressed(toUnits(event->position()));
    int box = -1;
    int handle = -1;
    int pressedImage = -1;
    const bool ctrl = event->modifiers() & Qt::ControlModifier;
    m_drag = ctrl ? Drag::Pan : hitTest(event->position().toPoint(), &box, &handle, &pressedImage);
    if (m_drag == Drag::Image)
        Q_EMIT imagePressed(pressedImage);
    m_dragStart = event->position().toPoint();
    m_dragUnits = toUnits(event->position());
    m_originAtStart = m_origin;
    m_dragBox = box;
    m_dragHandle = handle;
    m_drawnBox = QRect();
    if (m_drag == Drag::BoxMove || m_drag == Drag::BoxResize) {
        m_dragBoxStart = m_boxes.at(box).rect;
        Q_EMIT boxPressed(box);
    } else if (m_drag != Drag::BoxDraw && !ctrl) {
        Q_EMIT boxPressed(-1);
    }
}

void GraphicsView::mouseMoveEvent(QMouseEvent* event)
{
    const QPointF pos = event->position();
    // Rounded down like Fighter Factory, so -0.5 is unit -1.
    const QPointF under = toUnits(pos);
    Q_EMIT cursorMoved(QPoint(int(std::floor(under.x())), int(std::floor(under.y()))));
    switch (m_drag) {
    case Drag::None:
        switch (hitTest(pos.toPoint())) {
        case Drag::VerticalGuide:
            setCursor(Qt::SizeHorCursor);
            break;
        case Drag::HorizontalGuide:
            setCursor(Qt::SizeVerCursor);
            break;
        case Drag::Image:
            setCursor(Qt::SizeAllCursor);
            break;
        default:
            unsetCursor();
        }
        break;
    case Drag::Pan:
        setOrigin(m_originAtStart + (pos.toPoint() - m_dragStart));
        break;
    case Drag::Image: {
        const QPointF units = toUnits(pos);
        const int dx = int(std::floor(units.x()) - std::floor(m_dragUnits.x()));
        const int dy = int(std::floor(units.y()) - std::floor(m_dragUnits.y()));
        if (dx != 0 || dy != 0) {
            m_dragUnits += QPointF(dx, dy);
            Q_EMIT imageDragged(dx, dy);
        }
        break;
    }
    case Drag::VerticalGuide:
        setGuides(QPoint(int(std::lround(toUnits(pos).x())), m_guidePosition.y()));
        break;
    case Drag::HorizontalGuide:
        setGuides(QPoint(m_guidePosition.x(), int(std::lround(toUnits(pos).y()))));
        break;
    case Drag::BoxMove:
    case Drag::BoxResize: {
        if (m_dragBox < 0 || m_dragBox >= m_boxes.size())
            break;
        const QPointF units = toUnits(pos);
        const int dx = int(std::floor(units.x()) - std::floor(m_dragUnits.x()));
        const int dy = int(std::floor(units.y()) - std::floor(m_dragUnits.y()));
        QRect r = m_dragBoxStart;
        if (m_drag == Drag::BoxMove) {
            r.translate(dx, dy);
        } else {
            int left = r.left(), top = r.top(), right = r.right(), bottom = r.bottom();
            const int h = m_dragHandle;
            if (h == 0 || h == 6 || h == 7)
                left += dx;
            if (h == 2 || h == 3 || h == 4)
                right += dx;
            if (h == 0 || h == 1 || h == 2)
                top += dy;
            if (h == 4 || h == 5 || h == 6)
                bottom += dy;
            r.setCoords(left, top, right, bottom);
        }
        m_boxes[m_dragBox].rect = r;
        update();
        break;
    }
    case Drag::BoxDraw: {
        const QPointF start = m_dragUnits;
        const QPointF units = toUnits(pos);
        QRect r;
        r.setCoords(int(std::floor(std::min(start.x(), units.x()))), int(std::floor(std::min(start.y(), units.y()))),
                    int(std::floor(std::max(start.x(), units.x()))), int(std::floor(std::max(start.y(), units.y()))));
        m_drawnBox = r;
        update();
        break;
    }
    }
}

void GraphicsView::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_drag != Drag::None) {
        const Drag finished = m_drag;
        m_drag = Drag::None;
        if (finished == Drag::Image)
            Q_EMIT dragFinished();
        if ((finished == Drag::BoxMove || finished == Drag::BoxResize) && m_dragBox >= 0 &&
            m_dragBox < m_boxes.size() && m_boxes.at(m_dragBox).rect != m_dragBoxStart) {
            m_boxes[m_dragBox].rect = m_boxes.at(m_dragBox).rect.normalized();
            Q_EMIT boxEdited(m_dragBox, m_boxes.at(m_dragBox).rect);
        }
        if (finished == Drag::BoxDraw) {
            const QRect drawn = m_drawnBox;
            m_drawnBox = QRect();
            update();
            if (drawn.isValid() && (event->position().toPoint() - m_dragStart).manhattanLength() > 2)
                Q_EMIT boxDrawn(drawn);
        }
    }
    QWidget::mouseReleaseEvent(event);
}

void GraphicsView::keyPressEvent(QKeyEvent* event)
{
    if (selectedBox() >= 0 && event->modifiers() == Qt::NoModifier) {
        switch (event->key()) {
        case Qt::Key_Left:
            Q_EMIT boxNudged(-1, 0);
            return;
        case Qt::Key_Right:
            Q_EMIT boxNudged(1, 0);
            return;
        case Qt::Key_Up:
            Q_EMIT boxNudged(0, -1);
            return;
        case Qt::Key_Down:
            Q_EMIT boxNudged(0, 1);
            return;
        case Qt::Key_Delete:
            Q_EMIT boxDeleteRequested();
            return;
        default:
            break;
        }
    }
    QWidget::keyPressEvent(event);
}

void GraphicsView::wheelEvent(QWheelEvent* event)
{
    const int steps = m_wheel.take(event);
    if (steps == 0)
        return;
    zoomTo(m_zoomIndex + steps, zoomAnchor());
    event->accept();
}

} // namespace OFS
