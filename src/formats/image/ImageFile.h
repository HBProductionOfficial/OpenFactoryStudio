#pragma once

#include "formats/FormatsExport.h"
#include "formats/image/Image.h"
#include "formats/util/Bytes.h"
#include "formats/util/Result.h"

namespace OFS::Formats {

enum class ImageFileType
{
    Unknown,
    Png,
    Gif,
    Bmp,
    Pcx,
};

OFS_FORMATS_EXPORT ImageFileType imageFileType(ByteSpan data);

// 8-bit images keep their palette and exact indices.
OFS_FORMATS_EXPORT Result<Image> decodeImageFile(ByteSpan data);

} // namespace OFS::Formats
