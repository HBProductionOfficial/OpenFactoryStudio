#pragma once

#include "formats/FormatsExport.h"
#include "formats/image/Image.h"

namespace OFS::Formats {

// "Convert to 8-bit". Index 0 is kept for transparent pixels. Up to 31 colors give a 32-color
// palette, more are cut to 255 by median cut.
OFS_FORMATS_EXPORT Image quantize(const Image& image);

} // namespace OFS::Formats
