#pragma once

#include "formats/FormatsExport.h"
#include "formats/codecs/DecodeMode.h"
#include "formats/util/Bytes.h"
#include "formats/util/Result.h"

#include <cstddef>
#include <cstdint>
#include <span>

namespace OFS::Formats {

// GIF's LZW. Codes start at `minCodeSize` + 1 bits, grow up to 12 bits and are packed lowest bit
// first. Code 2^minCodeSize clears the dictionary and the next code ends the stream. The caller
// splits the stream into GIF sub-blocks.

OFS_FORMATS_EXPORT Result<Bytes> encodeGifLzw(std::span<const std::uint8_t> indices, int minCodeSize);

OFS_FORMATS_EXPORT Result<std::size_t> decodeGifLzw(ByteSpan data, int minCodeSize, std::span<std::uint8_t> out,
                                                    DecodeMode mode = DecodeMode::Strict);

} // namespace OFS::Formats
