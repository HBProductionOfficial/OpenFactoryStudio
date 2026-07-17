#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace OFS::Formats {

using Bytes = std::vector<std::byte>;
using ByteSpan = std::span<const std::byte>;

inline ByteSpan asBytes(std::string_view text) noexcept
{
    return std::as_bytes(std::span(text.data(), text.size()));
}

inline ByteSpan asBytes(std::span<const std::uint8_t> data) noexcept
{
    return std::as_bytes(data);
}

inline Bytes toBytes(std::string_view text)
{
    const ByteSpan view = asBytes(text);
    return Bytes(view.begin(), view.end());
}

inline Bytes toBytes(std::span<const std::uint8_t> data)
{
    const ByteSpan view = std::as_bytes(data);
    return Bytes(view.begin(), view.end());
}

// No decoding. Each byte becomes one char.
inline std::string toText(ByteSpan data)
{
    return std::string(reinterpret_cast<const char*>(data.data()), data.size());
}

inline std::span<const std::uint8_t> asU8(ByteSpan data) noexcept
{
    return {reinterpret_cast<const std::uint8_t*>(data.data()), data.size()};
}

} // namespace OFS::Formats
