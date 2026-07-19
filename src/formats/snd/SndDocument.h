#pragma once

#include "formats/FormatsExport.h"
#include "formats/util/Bytes.h"
#include "formats/util/Result.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

namespace OFS::Formats {

struct SndSound
{
    std::int32_t group = 0;
    std::int32_t number = 0;
    Bytes data;
};

// Elecbyte SND archive. Kept as stored, so an unchanged file saves back byte for byte.
class OFS_FORMATS_EXPORT SndDocument
{
public:
    // Real files use 00 01 00 01, 01 00 01 00 and 04 00 00 00.
    std::array<std::uint8_t, 4> versionBytes = {0, 1, 0, 1};
    std::vector<SndSound> sounds;
    // Header bytes 24-511, often used for a comment.
    std::array<std::uint8_t, 488> comment{};

    static Result<SndDocument> read(ByteSpan data);
    static Result<SndDocument> load(const std::filesystem::path& path);
    Bytes write() const;
    Status save(const std::filesystem::path& path) const;

    std::optional<std::size_t> find(std::int32_t group, std::int32_t number) const;
};

} // namespace OFS::Formats
