#pragma once

#include "core/CoreExport.h"
#include "formats/image/Image.h"

#include <QImage>

namespace OFS {

OFS_CORE_EXPORT QImage toQImage(const Formats::Image& rgba, QImage::Format format = QImage::Format_RGBA8888);

} // namespace OFS
