#include "core/sprites/QtImages.h"

namespace OFS {

QImage toQImage(const Formats::Image& rgba, QImage::Format format)
{
    if (rgba.pixelBuffer().empty())
        return {};
    // A view of the pixels, then a copy that owns them.
    const QImage view(rgba.pixels().data(), rgba.width(), rgba.height(), rgba.width() * 4, QImage::Format_RGBA8888);
    return format == QImage::Format_RGBA8888 ? view.copy() : view.convertToFormat(format);
}

} // namespace OFS
