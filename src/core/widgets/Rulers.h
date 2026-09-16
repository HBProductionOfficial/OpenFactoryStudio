#pragma once

#include "core/CoreExport.h"

#include <QColor>
#include <QRect>

class QPainter;
class QPalette;

namespace OFS::Rulers {

struct OFS_CORE_EXPORT Colours
{
    QColor band;
    QColor strip;
    QColor ink;

    static Colours from(const QPalette& palette);
};

// Where unit 0 falls in widget pixels, and pixels per unit (negative for a reversed axis).
struct Axis
{
    double zero = 0;
    double scale = 1;
};

struct Metrics
{
    int thickness = 20;
    int tickStrip = 6;
    int fontPixels = 9;
};

// A number every `major` units (a multiple of 5 near 50 / zoom), five ticks per number.
OFS_CORE_EXPORT void steps(double zoom, int* minor, int* major);

OFS_CORE_EXPORT void draw(QPainter& p, const QRect& bounds, const QRect& area, const Axis& x, const Axis& y, int minor,
                          int major, const Colours& colours, const Metrics& metrics = {});

} // namespace OFS::Rulers
