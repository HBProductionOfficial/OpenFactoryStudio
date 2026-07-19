#pragma once

#include <cstdint>

namespace OFS::Formats {

enum class DecodeMode : std::uint8_t
{
    Strict,
    // Decodes what it can, like game engines do, because a few real SFF files are damaged.
    Lenient,
};

} // namespace OFS::Formats
