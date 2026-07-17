#pragma once

#include "formats/FormatsExport.h"
#include "formats/util/Bytes.h"
#include "formats/util/Result.h"

#include <filesystem>

namespace OFS::Formats {

OFS_FORMATS_EXPORT Result<Bytes> readFile(const std::filesystem::path& path);

// Writes to a temporary file next to the target and renames it over, so an existing file is
// never left half-written.
OFS_FORMATS_EXPORT Status writeFile(const std::filesystem::path& path, ByteSpan data);

} // namespace OFS::Formats
