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

// SFF v2 RLE8 (sprite format 2). A byte 0x40-0x7F starts a run: its low six bits are the length
// and the next byte is the pixel. Any other byte is a single pixel, so pixels 0x40-0x7F always
// need a run. `data` has no 4-byte size prefix.
OFS_FORMATS_EXPORT Result<std::vector<std::uint8_t>> decodeRle8(ByteSpan data, std::size_t pixelCount,
                                                                DecodeMode mode = DecodeMode::Strict);

OFS_FORMATS_EXPORT Bytes encodeRle8(std::span<const std::uint8_t> pixels);

} // namespace OFS::Formats
