#include "formats/image/ImageFile.h"

#include "formats/codecs/Bmp.h"
#include "formats/codecs/Gif.h"
#include "formats/codecs/Pcx.h"
#include "formats/codecs/Png.h"

namespace OFS::Formats {

ImageFileType imageFileType(ByteSpan data)
{
    auto at = [&data](std::size_t i) {
        return i < data.size() ? std::to_integer<unsigned>(data[i]) : 0u;
    };
    if (data.size() >= 8 && at(0) == 0x89 && at(1) == 'P' && at(2) == 'N' && at(3) == 'G')
        return ImageFileType::Png;
    if (data.size() >= 6 && at(0) == 'G' && at(1) == 'I' && at(2) == 'F' && at(3) == '8')
        return ImageFileType::Gif;
    if (data.size() >= 14 && at(0) == 'B' && at(1) == 'M')
        return ImageFileType::Bmp;
    // PCX: manufacturer 10, version 0-5, RLE encoding 1.
    if (data.size() >= 128 && at(0) == 0x0A && at(1) <= 5 && at(2) == 1)
        return ImageFileType::Pcx;
    return ImageFileType::Unknown;
}

Result<Image> decodeImageFile(ByteSpan data)
{
    switch (imageFileType(data)) {
    case ImageFileType::Png:
        return decodePng(data);
    case ImageFileType::Gif:
        return decodeGif(data, DecodeMode::Lenient);
    case ImageFileType::Bmp:
        return decodeBmp(data, DecodeMode::Lenient);
    case ImageFileType::Pcx:
        return decodePcx(data, PcxPalette::Optional, DecodeMode::Lenient);
    case ImageFileType::Unknown:
        break;
    }
    return Error{"not a PNG, GIF, BMP or PCX image"};
}

} // namespace OFS::Formats
