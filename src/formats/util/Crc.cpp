#include "formats/util/Crc.h"

#include <array>

namespace OFS::Formats {

namespace {

template <typename T, T Poly>
constexpr std::array<T, 256> makeTable()
{
    std::array<T, 256> table{};
    for (unsigned i = 0; i < 256; ++i) {
        T c = i;
        for (int k = 0; k < 8; ++k)
            c = (c & 1) ? (c >> 1) ^ Poly : (c >> 1);
        table[i] = c;
    }
    return table;
}

constexpr auto kCrc32Table = makeTable<std::uint32_t, 0xEDB88320u>();
constexpr auto kCrc64Table = makeTable<std::uint64_t, 0xC96C5795D7870F42ull>();

} // namespace

std::uint32_t crc32(std::span<const std::byte> data, std::uint32_t crc)
{
    crc = ~crc;
    for (std::byte b : data)
        crc = kCrc32Table[(crc ^ static_cast<std::uint8_t>(b)) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

std::uint64_t crc64(std::span<const std::byte> data, std::uint64_t crc)
{
    crc = ~crc;
    for (std::byte b : data)
        crc = kCrc64Table[(crc ^ static_cast<std::uint8_t>(b)) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

} // namespace OFS::Formats
