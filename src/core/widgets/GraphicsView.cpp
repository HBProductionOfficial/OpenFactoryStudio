#include "core/widgets/GraphicsView.h"

#include <QKeyEvent>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>

#include <cmath>

namespace OFS {

namespace {

constexpr int RulerSize = 20;
constexpr int GuideGrab = 3;
constexpr int HandleSize = 7;

void rulerSteps(double zoom, int* minor, int* major)
{
    *major = std::max(5, int(std::floor(50.0 / zoom / 5.0)) * 5);
    *minor = std::max(1, *major / 5);
}

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
    QRect bounds;
    for (const Image& image : std::as_const(m_images))
        bounds |= image.transform.mapRect(QRect(image.topLeft, image.image.size()));
    if (bounds.isEmpty())
        return;
    const QRect area = contentRect().adjusted(8, 8, -8, -8);
    int best = 0;
    for (int i = 0; i < m_zoomLevels.size(); ++i) {
        if (bounds.width() * m_zoomLevels[i] <= area.width() && bounds.height() * m_zoomLevels[i] <= area.height())
            best = i;
    }
    m_zoomIndex = best;
    const QPointF centre = QRectF(bounds).center() * zoom();
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
    setOrigin(contentRect().center());
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
        p.fillRect(area, m_backgroundColor);
        break;
    case Background::Checker: {
        const int cell = 8;
        p.fillRect(area, QColor(204, 204, 204));
        for (int y = area.top(); y <= area.bottom(); y += cell) {
            for (int x = area.left() + (((y - area.top()) / cell) % 2) * cell; x <= area.right(); x += 2 * cell)
                p.fillRect(QRect(x, y, cell, cell).intersected(area), QColor(240, 240, 240));
        }
        break;
    }
    case Background::Picture:
        p.fillRect(area, m_backgroundColor);
        if (!m_backgroundPicture.isNull()) {
            const QPointF at = toWidget(m_backgroundOffset);
            p.drawImage(QRectF(at, QSizeF(m_backgroundPicture.size()) * zoom()), m_backgroundPicture);
        }
        break;
    }
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

void GraphicsView::drawCross(QPainter& p, const QPointF& at, const QColor& fill, const QColor& outlineColor)
{
    // Default cross: a 13 x 13 plus with a hollow centre and an outline, scaled with the zoom.
    auto cells = [this, at](double x, double y, double w, double h) {
        return QRectF(toWidget(QPointF(at.x() + x, at.y() + y)), QSizeF(w, h) * zoom());
    };
    QPainterPath outline;
    outline.addRect(cells(-7, -1, 15, 3));
    outline.addRect(cells(-1, -7, 3, 15));
    outline.setFillRule(Qt::WindingFill);
    QPainterPath centre;
    centre.addRect(cells(0, 0, 1, 1));
    p.fillPath(outline.simplified().subtracted(centre), outlineColor);
    for (const QRectF& arm : {cells(-6, 0, 6, 1), cells(1, 0, 6, 1), cells(0, -6, 1, 6), cells(0, 1, 1, 6)})
        p.fillRect(arm, fill);
}

void GraphicsView::drawBoxes(QPainter& p)
{
    const double thickness = std::max(1.0, zoom());
    auto draw = [&](const QRect& units, const QColor& color, bool selected) {
        const QRectF r = boxRect(units);
        QColor fill = color;
        fill.setAlpha(64);
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
    const QColor band(38, 38, 38);
    const QColor strip(70, 70, 70);
    const QColor ink(200, 200, 200);
    const int tickStrip = 6;
    p.fillRect(QRect(0, 0, width(), RulerSize), band);
    p.fillRect(QRect(0, 0, RulerSize, height()), band);
    p.fillRect(QRect(RulerSize, RulerSize - tickStrip, width(), tickStrip), strip);
    p.fillRect(QRect(RulerSize - tickStrip, RulerSize, tickStrip, height()), strip);
    p.setPen(ink);
    QFont font = p.font();
    font.setPixelSize(9);
    p.setFont(font);
    int minor = 10;
    int major = 50;
    rulerSteps(zoom(), &minor, &major);
    const QRect area = contentRect();
    const int firstX = int(std::floor(toUnits(QPointF(area.left(), 0)).x() / minor)) * minor;
    for (int u = firstX;; u += minor) {
        const double x = toWidget(QPointF(u, 0)).x();
        if (x > area.right())
            break;
        if (x < area.left())
            continue;
        const bool labelled = u % major == 0;
        p.drawLine(QPointF(x, RulerSize - (labelled ? tickStrip : tickStrip / 2)), QPointF(x, RulerSize - 1));
        if (labelled)
            p.drawText(QRectF(x - 30, 0, 60, RulerSize - tickStrip), Qt::AlignCenter, QString::number(u));
    }
    const int firstY = int(std::floor(toUnits(QPointF(0, area.top())).y() / minor)) * minor;
    for (int u = firstY;; u += minor) {
        const double y = toWidget(QPointF(0, u)).y();
        if (y > area.bottom())
            break;
        if (y < area.top())
            continue;
        const bool labelled = u % major == 0;
        p.drawLine(QPointF(RulerSize - (labelled ? tickStrip : tickStrip / 2), y), QPointF(RulerSize - 1, y));
        if (labelled) {
            p.save();
            p.translate(0, y);
            p.rotate(-90);
            p.drawText(QRectF(-30, 0, 60, RulerSize - tickStrip), Qt::AlignCenter, QString::number(u));
            p.restore();
        }
    }
    p.fillRect(QRect(0, 0, RulerSize, RulerSize), band);
}

void GraphicsView::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter p(this);
    const QRect area = contentRect();
    p.save();
    p.setClipRect(area);
    // Back to front: background, images, boxes, crosses, guides. The two "over" options swap the
    // guides with the images and the boxes with the crosses.
    drawBackground(p, area);
    if (m_imagesOverGuides)
        drawGuides(p, area);
    drawImages(p);
    if (!m_boxesOverCrosses) {
        drawBoxes(p);
        drawFrames(p);
    }
    if (m_mainCross)
        drawCross(p, QPointF(m_mainCrossPosition), Qt::black, Qt::white);
    if (m_extraCross)
        drawCross(p, QPointF(0, 0), Qt::white, QColor(0, 160, 0));
    if (m_secondaryCross)
        drawCross(p, QPointF(m_secondaryPosition), Qt::white, QColor(0, 0, 255));
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
    m_drag = (event->modifiers() & Qt::ControlModifier)
                 ? Drag::Pan
                 : hitTest(event->position().toPoint(), &box, &handle, &pressedImage);
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
    } else if (m_drag != Drag::BoxDraw) {
        Q_EMIT boxPressed(-1);
    }
}

void GraphicsView::mouseMoveEvent(QMouseEvent* event)
{
    const QPointF pos = event->position();
    Q_EMIT cursorMoved(toUnits(pos).toPoint());
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
    const int steps = event->angleDelta().y() / 120;
    if (steps == 0)
        return;
    zoomTo(m_zoomIndex + steps, zoomAnchor());
    event->accept();
}

} // namespace OFS
