#include "core/widgets/Rulers.h"

#include <QPainter>
#include <QPalette>

#include <algorithm>
#include <cmath>

namespace OFS::Rulers {

Colours Colours::from(const QPalette& palette)
{
    const QColor window = palette.color(QPalette::Window);
    const bool dark = window.lightness() < 128;
    Colours colours;
    colours.band = dark ? window.darker(135) : window.darker(108);
    colours.strip = dark ? window.lighter(137) : window.darker(125);
    colours.ink = palette.color(QPalette::WindowText);
    return colours;
}

void steps(double zoom, int* minor, int* major)
{
    *major = std::max(5, int(std::floor(50.0 / zoom / 5.0)) * 5);
    *minor = std::max(1, *major / 5);
}

namespace {

template <typename Visit>
void forEachTick(const Axis& axis, double from, double to, int minor, Visit visit)
{
    if (axis.scale == 0 || minor <= 0)
        return;
    const double a = (from - axis.zero) / axis.scale;
    const double b = (to - axis.zero) / axis.scale;
    const int first = int(std::floor(std::min(a, b) / minor)) * minor;
    const double last = std::max(a, b);
    for (int unit = first; unit <= last; unit += minor) {
        const double pixel = axis.zero + unit * axis.scale;
        if (pixel >= from && pixel <= to)
            visit(unit, pixel);
    }
}

} // namespace

void draw(QPainter& p, const QRect& bounds, const QRect& area, const Axis& x, const Axis& y, int minor, int major,
          const Colours& colours, const Metrics& metrics)
{
    const int size = metrics.thickness;
    const int strip = metrics.tickStrip;
    p.save();
    p.fillRect(QRect(bounds.left(), bounds.top(), bounds.width(), size), colours.band);
    p.fillRect(QRect(bounds.left(), bounds.top(), size, bounds.height()), colours.band);
    p.fillRect(QRect(bounds.left() + size, bounds.top() + size - strip, bounds.width() - size, strip), colours.strip);
    p.fillRect(QRect(bounds.left() + size - strip, bounds.top() + size, strip, bounds.height() - size), colours.strip);
    p.setPen(colours.ink);
    QFont font = p.font();
    font.setPixelSize(metrics.fontPixels);
    p.setFont(font);
    const int top = bounds.top();
    const int left = bounds.left();
    forEachTick(x, area.left(), area.right(), minor, [&](int unit, double px) {
        const bool labelled = unit % major == 0;
        p.drawLine(QPointF(px, top + size - (labelled ? strip : strip / 2)), QPointF(px, top + size - 1));
        if (labelled)
            p.drawText(QRectF(px - 30, top, 60, size - strip), Qt::AlignCenter, QString::number(unit));
    });
    forEachTick(y, area.top(), area.bottom(), minor, [&](int unit, double py) {
        const bool labelled = unit % major == 0;
        p.drawLine(QPointF(left + size - (labelled ? strip : strip / 2), py), QPointF(left + size - 1, py));
        if (labelled) {
            p.save();
            p.translate(left, py);
            p.rotate(-90);
            p.drawText(QRectF(-30, 0, 60, size - strip), Qt::AlignCenter, QString::number(unit));
            p.restore();
        }
    });
    p.fillRect(QRect(left, top, size, size), colours.band);
    p.restore();
}

} // namespace OFS::Rulers
