#pragma once

#include <cstddef>
#include <cstdint>

namespace OFS::Formats {

enum class DecodeMode : std::uint8_t
{
    Strict,
    // Decodes what it can, like game engines do, because a few real SFF files are damaged.
    Lenient,
};

// Lenient decoding fills in what damaged data leaves out, but past this many pixels only when the
// data could hold the image, so a few bytes can't stand for a huge one.
inline constexpr std::size_t kMaxPaddedPixels = std::size_t{1} << 20;

} // namespace OFS::Formats
