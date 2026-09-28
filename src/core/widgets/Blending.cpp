#include "core/widgets/Blending.h"

#include <QImage>
#include <QRectF>

namespace OFS {

namespace {

// QPainter's Plus clips the sum before it applies the opacity, so weight the colours first.
QImage weighted(const QImage& image, double opacity)
{
    QImage result(image.size(), QImage::Format_ARGB32_Premultiplied);
    result.setDevicePixelRatio(image.devicePixelRatio());
    result.fill(Qt::transparent);
    QPainter p(&result);
    p.setOpacity(opacity);
    p.drawImage(QPointF(0, 0), image);
    return result;
}

} // namespace

void drawBlended(QPainter& p, const QRectF& target, const QImage& image, QPainter::CompositionMode mode, double opacity)
{
    const bool adds = mode == QPainter::CompositionMode_Plus || mode == QPainter::CompositionMode_Difference;
    const QImage& source = adds && opacity < 1.0 ? weighted(image, opacity) : image;
    if (adds)
        opacity = 1.0;
    if (mode != QPainter::CompositionMode_Difference) {
        p.setCompositionMode(mode);
        p.setOpacity(opacity);
        p.drawImage(target, source);
        return;
    }
    // dst - src = 1 - ((1 - dst) + src), with the addition saturating at white.
    p.setOpacity(1.0);
    p.setCompositionMode(QPainter::CompositionMode_Difference);
    p.fillRect(target, Qt::white);
    p.setCompositionMode(QPainter::CompositionMode_Plus);
    p.drawImage(target, source);
    p.setCompositionMode(QPainter::CompositionMode_Difference);
    p.fillRect(target, Qt::white);
}

} // namespace OFS
