#pragma once

#include "formats/FormatsExport.h"

#include <cstddef>
#include <cstdint>
#include <span>

namespace OFS::Formats {

// CRC-32 (IEEE 802.3, reflected, polynomial 0xEDB88320) as used by PNG and ZIP.
OFS_FORMATS_EXPORT std::uint32_t crc32(std::span<const std::byte> data, std::uint32_t crc = 0);

// CRC-64 (ECMA-182, reflected, polynomial 0xC96C5795D7870F42) as used by XZ.
OFS_FORMATS_EXPORT std::uint64_t crc64(std::span<const std::byte> data, std::uint64_t crc = 0);

} // namespace OFS::Formats
