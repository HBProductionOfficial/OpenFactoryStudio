#pragma once

#include "formats/FormatsExport.h"
#include "formats/codecs/DecodeMode.h"
#include "formats/util/Bytes.h"
#include "formats/util/Result.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace OFS::Formats {

// SFF v2 RLE5 (sprite format 3), for palette indices 0-31. Each packet is:
//   byte 0  R: the first color is written R + 1 times
//   byte 1  bit 7 set: a color byte follows, else the first color is 0. Bits 0-6: N extra runs
//   N bytes bits 5-7 = length - 1, bits 0-4 = color
// `data` has no 4-byte size prefix.
OFS_FORMATS_EXPORT Result<std::vector<std::uint8_t>> decodeRle5(ByteSpan data, std::size_t pixelCount,
                                                                DecodeMode mode = DecodeMode::Strict);

OFS_FORMATS_EXPORT Result<Bytes> encodeRle5(std::span<const std::uint8_t> pixels);

} // namespace OFS::Formats
