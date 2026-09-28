#pragma once

#include "core/CoreExport.h"

#include <QPainter>

class QImage;
class QRectF;

namespace OFS {

// Draws an image the way the engine blends it. CompositionMode_Difference stands for the engine's
// subtraction, done by inverting the opaque destination, adding the image and inverting back.
OFS_CORE_EXPORT void drawBlended(QPainter& p, const QRectF& target, const QImage& image, QPainter::CompositionMode mode,
                                 double opacity = 1.0);

} // namespace OFS
