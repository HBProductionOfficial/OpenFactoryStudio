#pragma once

#include "formats/FormatsExport.h"
#include "formats/image/Color.h"
#include "formats/util/Bytes.h"
#include "formats/util/Result.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace OFS::Formats {

enum class ActOrder : std::uint8_t
{
    // MUGEN character palettes store the colors backwards: the first triplet is index 255.
    Reversed,
    Natural,
};

// 256 RGB triplets (768 bytes). Some files add a color count and transparent index (772 bytes),
// which are ignored.
OFS_FORMATS_EXPORT Result<Palette> readAct(ByteSpan data, ActOrder order);
OFS_FORMATS_EXPORT Bytes writeAct(const Palette& palette, ActOrder order);

// "RIFF" ... "PAL " with a LOGPALETTE in a "data" chunk.
OFS_FORMATS_EXPORT Result<Palette> readRiffPal(ByteSpan data);
OFS_FORMATS_EXPORT Bytes writeRiffPal(const Palette& palette);

// "JASC-PAL", "0100", the color count, then one "R G B" line per color.
OFS_FORMATS_EXPORT Result<Palette> readJascPal(std::string_view text);
OFS_FORMATS_EXPORT std::string writeJascPal(const Palette& palette);

// Corel Painter color set: one line per color starting with "R: r, G: g, B: b".
OFS_FORMATS_EXPORT Result<Palette> readPainterSwatches(std::string_view text);

// Photoshop .aco, version 1 and 2.
OFS_FORMATS_EXPORT Result<Palette> readAco(ByteSpan data);
OFS_FORMATS_EXPORT Bytes writeAco(const Palette& palette);

OFS_FORMATS_EXPORT Result<Palette> readAse(ByteSpan data);
OFS_FORMATS_EXPORT Bytes writeAse(const Palette& palette);

OFS_FORMATS_EXPORT Result<Palette> paletteFromPcx(ByteSpan data);
OFS_FORMATS_EXPORT Result<Palette> paletteFromPng(ByteSpan data);
OFS_FORMATS_EXPORT Result<Palette> paletteFromGif(ByteSpan data);
OFS_FORMATS_EXPORT Result<Palette> paletteFromBmp(ByteSpan data);

// Detects the format from the content. A 768 or 772 byte file that only looks like an image is
// read as ACT.
OFS_FORMATS_EXPORT Result<Palette> readPaletteFile(ByteSpan data, ActOrder actOrder = ActOrder::Reversed);

} // namespace OFS::Formats
