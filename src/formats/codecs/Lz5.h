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

// SFF v2 LZ5 (sprite format 4), for palette indices 0-31. Up to eight packets follow a control
// byte whose bits, lowest first, mark each one as a run (0) or a copy (1).
//   short run   1 byte: bits 5-7 = length (1-7), bits 0-4 = color
//   long run    2 bytes: color with bits 5-7 clear, then length - 8
//   short copy  bits 0-5 = length - 1, then a distance - 1 byte. Bits 6-7 are gathered two at a
//               time, highest first, into a spare byte. Every fourth short copy has no distance
//               byte and uses the spare byte instead.
//   long copy   3 bytes: bits 0-5 clear and bits 6-7 = distance bits 8-9, then distance - 1 bits
//               0-7, then length - 3
// `data` has no 4-byte size prefix. No control byte follows the last packet.
OFS_FORMATS_EXPORT Result<std::vector<std::uint8_t>> decodeLz5(ByteSpan data, std::size_t pixelCount,
                                                               DecodeMode mode = DecodeMode::Strict);

OFS_FORMATS_EXPORT Result<Bytes> encodeLz5(std::span<const std::uint8_t> pixels);

} // namespace OFS::Formats
