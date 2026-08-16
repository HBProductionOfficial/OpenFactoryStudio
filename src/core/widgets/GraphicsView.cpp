#include "core/widgets/GraphicsView.h"

#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

#include <cmath>

namespace OFS {

namespace {

constexpr int RulerSize = 20;
constexpr int GuideGrab = 3;

void rulerSteps(double zoom, int* minor, int* major)
{
    static const int steps[] = {1, 2, 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000};
    *major = steps[std::size(steps) - 1];
    for (int s : steps) {
        if (s * zoom >= 50) {
            *major = s;
            break;
        }
    }
    *minor = std::max(1, *major / 5);
    if (*minor * zoom < 4)
        *minor = *major;
}

} // namespace

GraphicsView::GraphicsView(QWidget* parent)
    : QWidget(parent)
    , m_zoomLevels{0.25, 0.5, 1, 2, 3, 4, 6, 8, 12, 16, 24, 32}
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

void GraphicsView::zoomIn()
{
    zoomTo(m_zoomIndex + 1, QPointF(m_origin));
}

void GraphicsView::zoomOut()
{
    zoomTo(m_zoomIndex - 1, QPointF(m_origin));
}

void GraphicsView::zoomReset()
{
    int best = 0;
    for (int i = 0; i < m_zoomLevels.size(); ++i) {
        if (std::abs(m_zoomLevels[i] - 1.0) < std::abs(m_zoomLevels[best] - 1.0))
            best = i;
    }
    zoomTo(best, QPointF(m_origin));
}

void GraphicsView::zoomFit()
{
    QRect bounds;
    for (const Image& image : std::as_const(m_images))
        bounds |= QRect(image.topLeft, image.image.size());
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
    p.setPen(QPen(QColor(0, 160, 255), 1, Qt::DashLine));
    p.drawLine(QPointF(at.x(), area.top()), QPointF(at.x(), area.bottom()));
    p.drawLine(QPointF(area.left(), at.y()), QPointF(area.right(), at.y()));
}

void GraphicsView::drawImages(QPainter& p)
{
    p.setRenderHint(QPainter::SmoothPixmapTransform, false);
    for (const Image& image : std::as_const(m_images)) {
        if (image.image.isNull())
            continue;
        p.setOpacity(image.opacity);
        p.drawImage(QRectF(toWidget(image.topLeft), QSizeF(image.image.size()) * zoom()), image.image);
    }
    p.setOpacity(1.0);
}

void GraphicsView::drawRulers(QPainter& p)
{
    if (!m_rulers)
        return;
    const QColor back = palette().color(QPalette::Window);
    p.fillRect(QRect(0, 0, width(), RulerSize), back);
    p.fillRect(QRect(0, 0, RulerSize, height()), back);
    p.setPen(palette().color(QPalette::WindowText));
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
        p.drawLine(QPointF(x, RulerSize - (labelled ? 8 : 4)), QPointF(x, RulerSize - 1));
        if (labelled)
            p.drawText(QRectF(x + 2, 0, 60, RulerSize - 6), Qt::AlignLeft | Qt::AlignVCenter, QString::number(u));
    }
    const int firstY = int(std::floor(toUnits(QPointF(0, area.top())).y() / minor)) * minor;
    for (int u = firstY;; u += minor) {
        const double y = toWidget(QPointF(0, u)).y();
        if (y > area.bottom())
            break;
        if (y < area.top())
            continue;
        const bool labelled = u % major == 0;
        p.drawLine(QPointF(RulerSize - (labelled ? 8 : 4), y), QPointF(RulerSize - 1, y));
        if (labelled) {
            p.save();
            p.translate(RulerSize - 8, y + 2);
            p.rotate(-90);
            p.drawText(QRectF(-60, -10, 60, 10), Qt::AlignRight | Qt::AlignVCenter, QString::number(u));
            p.restore();
        }
    }
    p.fillRect(QRect(0, 0, RulerSize, RulerSize), back);
}

void GraphicsView::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter p(this);
    const QRect area = contentRect();
    p.save();
    p.setClipRect(area);
    drawBackground(p, area);
    if (!m_imagesOverGuides)
        drawGuides(p, area);
    drawImages(p);
    if (m_imagesOverGuides)
        drawGuides(p, area);
    if (m_mainCross) {
        const QPoint at = m_origin;
        p.setPen(QPen(QColor(0, 0, 0, 170), 3));
        p.drawLine(at + QPoint(-8, 0), at + QPoint(8, 0));
        p.drawLine(at + QPoint(0, -8), at + QPoint(0, 8));
        p.setPen(QPen(Qt::white, 1));
        p.drawLine(at + QPoint(-8, 0), at + QPoint(8, 0));
        p.drawLine(at + QPoint(0, -8), at + QPoint(0, 8));
    }
    p.restore();
    drawRulers(p);
}

GraphicsView::Drag GraphicsView::hitTest(const QPoint& pos) const
{
    if (m_guides) {
        const QPointF at = toWidget(m_guidePosition);
        if (std::abs(pos.x() - at.x()) <= GuideGrab)
            return Drag::VerticalGuide;
        if (std::abs(pos.y() - at.y()) <= GuideGrab)
            return Drag::HorizontalGuide;
    }
    const QPointF units = toUnits(pos);
    for (qsizetype i = m_images.size() - 1; i >= 0; --i) {
        const Image& image = m_images[i];
        if (!image.draggable || image.image.isNull())
            continue;
        const QPoint local = (units - QPointF(image.topLeft)).toPoint();
        if (QRect(QPoint(), image.image.size()).contains(local) && image.image.pixelColor(local).alpha() > 0)
            return Drag::Image;
    }
    return Drag::Pan;
}

void GraphicsView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton || !contentRect().contains(event->position().toPoint())) {
        QWidget::mousePressEvent(event);
        return;
    }
    m_drag = hitTest(event->position().toPoint());
    m_dragStart = event->position().toPoint();
    m_dragUnits = toUnits(event->position());
    m_originAtStart = m_origin;
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
    }
}

void GraphicsView::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_drag != Drag::None) {
        const Drag finished = m_drag;
        m_drag = Drag::None;
        if (finished == Drag::Image)
            Q_EMIT dragFinished();
    }
    QWidget::mouseReleaseEvent(event);
}

void GraphicsView::wheelEvent(QWheelEvent* event)
{
    const int steps = event->angleDelta().y() / 120;
    if (steps == 0)
        return;
    const QPointF anchor = m_zoomAtCursor ? event->position() : QPointF(contentRect().center());
    zoomTo(m_zoomIndex + steps, anchor);
    event->accept();
}

} // namespace OFS
