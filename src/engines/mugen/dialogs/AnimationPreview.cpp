#include "engines/mugen/dialogs/AnimationPreview.h"

#include "core/widgets/Rulers.h"

#include <QLinearGradient>
#include <QPainter>

#include <cmath>

namespace OFS::Mugen {

namespace {

constexpr int RulerSize = 22;
constexpr int GridStep = 40;
constexpr int TickStep = 10;
constexpr int LabelStep = 50;

} // namespace

AnimationPreview::AnimationPreview(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(320, 200);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
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
    return {std::max(x, double(RulerSize) + 24), std::max(y, double(RulerSize) + 24)};
}

QPointF AnimationPreview::toWidget(const QPointF& units) const
{
    return origin() + units;
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
    const double firstX = zero.x() - std::floor((zero.x() - view.left()) / GridStep) * GridStep;
    for (double x = firstX; x <= view.right(); x += GridStep)
        p.drawLine(QPointF(x, view.top()), QPointF(x, view.bottom()));
    const double firstY = zero.y() - std::floor((zero.y() - view.top()) / GridStep) * GridStep;
    for (double y = firstY; y <= view.bottom(); y += GridStep)
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
        const QRectF target(at.x() - ax * figure.scale, at.y() - ay * figure.scale, image.width() * figure.scale,
                            image.height() * figure.scale);
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

    Rulers::draw(p, rect(), view, {zero.x(), 1}, {zero.y(), 1}, TickStep, LabelStep, Rulers::Colours::from(palette()),
                 {RulerSize, 8, 10});
}

} // namespace OFS::Mugen
