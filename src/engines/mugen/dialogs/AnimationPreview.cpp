#include "engines/mugen/dialogs/AnimationPreview.h"

#include "core/widgets/GraphicsView.h"
#include "core/widgets/Rulers.h"

#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace OFS::Mugen {

namespace {

constexpr int RulerSize = 22;
constexpr int GridStep = 40;

} // namespace

AnimationPreview::AnimationPreview(QWidget* parent)
    : QWidget(parent)
    , m_levels(GraphicsView::largeZoomRange())
    , m_zoomIndex(int(std::max<qsizetype>(0, m_levels.indexOf(1.0))))
{
    setMinimumSize(320, 200);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setCursor(Qt::OpenHandCursor);
}

void AnimationPreview::setZoomIndex(int index)
{
    index = std::clamp(index, 0, int(m_levels.size()) - 1);
    if (index == m_zoomIndex)
        return;
    m_zoomIndex = index;
    update();
}

void AnimationPreview::wheelEvent(QWheelEvent* event)
{
    const int steps = m_wheel.take(event);
    if (steps != 0)
        setZoomIndex(m_zoomIndex + steps);
    event->accept();
}

void AnimationPreview::setFigures(const QList<Figure>& figures)
{
    m_figures = figures;
    update();
}

void AnimationPreview::setScreenSize(const QSize& size)
{
    m_screen = size;
    updateGeometry();
    update();
}

QSize AnimationPreview::sizeHint() const
{
    return {qMax(782, m_screen.width() - 22), qMax(200, m_screen.height() - 28)};
}

QRect AnimationPreview::viewport() const
{
    return rect().adjusted(RulerSize, RulerSize, 0, 0);
}

QPointF AnimationPreview::origin() const
{
    // The screen puts the players half way across and on the floor, at 170/240 of its height.
    const double x = std::min(m_screen.width() / 2.0, width() / 2.0);
    const double y = std::min(std::round(m_screen.height() * 170.0 / 240.0) + 1, height() - 24.0);
    return QPointF(std::max(x, double(RulerSize) + 24), std::max(y, double(RulerSize) + 24)) + m_pan;
}

void AnimationPreview::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton)
        return QWidget::mousePressEvent(event);
    m_panning = true;
    m_panStart = event->position().toPoint();
    m_panAtStart = m_pan;
}

void AnimationPreview::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_panning)
        return QWidget::mouseMoveEvent(event);
    // The hand closes once the view moves.
    const QPoint delta = event->position().toPoint() - m_panStart;
    if (!delta.isNull() && cursor().shape() != Qt::ClosedHandCursor)
        setCursor(Qt::ClosedHandCursor);
    m_pan = m_panAtStart + delta;
    update();
}

void AnimationPreview::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton || !m_panning)
        return QWidget::mouseReleaseEvent(event);
    m_panning = false;
    setCursor(Qt::OpenHandCursor);
}

QPointF AnimationPreview::toWidget(const QPointF& units) const
{
    return origin() + units * zoom();
}

void AnimationPreview::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter p(this);
    const QRect view = viewport();
    const QPointF zero = origin();

    // Backdrop: sky and floor, with a grid aligned on the origin.
    QLinearGradient sky(view.topLeft(), QPointF(view.left(), zero.y()));
    sky.setColorAt(0, QColor(46, 64, 92));
    sky.setColorAt(1, QColor(78, 110, 150));
    p.fillRect(QRectF(view.left(), view.top(), view.width(), zero.y() - view.top()), sky);
    QLinearGradient floor(QPointF(view.left(), zero.y()), view.bottomLeft());
    floor.setColorAt(0, QColor(70, 100, 140));
    floor.setColorAt(1, QColor(98, 136, 182));
    p.fillRect(QRectF(view.left(), zero.y(), view.width(), view.bottom() - zero.y() + 1), floor);
    p.save();
    p.setClipRect(view);
    p.setPen(QColor(150, 185, 225, 90));
    const double scale = zoom();
    const double grid = GridStep * scale;
    const double firstX = zero.x() - std::floor((zero.x() - view.left()) / grid) * grid;
    for (double x = firstX; x <= view.right(); x += grid)
        p.drawLine(QPointF(x, view.top()), QPointF(x, view.bottom()));
    const double firstY = zero.y() - std::floor((zero.y() - view.top()) / grid) * grid;
    for (double y = firstY; y <= view.bottom(); y += grid)
        p.drawLine(QPointF(view.left(), y), QPointF(view.right(), y));
    p.setPen(QColor(190, 215, 240, 160));
    p.drawLine(QPointF(view.left(), zero.y()), QPointF(view.right(), zero.y()));

    for (const Figure& figure : std::as_const(m_figures)) {
        if (figure.image.isNull())
            continue;
        const QImage image = figure.image.mirrored(figure.flipH, figure.flipV);
        const double ax = figure.flipH ? image.width() - figure.axis.x() : figure.axis.x();
        const double ay = figure.flipV ? image.height() - figure.axis.y() : figure.axis.y();
        const QPointF at = toWidget(figure.position);
        const double size = figure.scale * scale;
        const QRectF target(at.x() - ax * size, at.y() - ay * size, image.width() * size, image.height() * size);
        p.setRenderHint(QPainter::SmoothPixmapTransform, false);
        p.drawImage(target, image);
    }
    for (const Figure& figure : std::as_const(m_figures)) {
        const QPointF at = toWidget(figure.position);
        const int c = 7;
        p.setPen(QPen(QColor(0, 0, 0, 160), 3));
        p.drawLine(at + QPointF(-c, 0), at + QPointF(c, 0));
        p.drawLine(at + QPointF(0, -c), at + QPointF(0, c));
        p.setPen(QPen(figure.cross, 1));
        p.drawLine(at + QPointF(-c, 0), at + QPointF(c, 0));
        p.drawLine(at + QPointF(0, -c), at + QPointF(0, c));
    }
    p.restore();

    int minor = 0;
    int major = 0;
    Rulers::steps(scale, &minor, &major);
    Rulers::draw(p, rect(), view, {zero.x(), scale}, {zero.y(), scale}, minor, major, Rulers::Colors::from(palette()),
                 {RulerSize, 8, 10});
}

} // namespace OFS::Mugen
