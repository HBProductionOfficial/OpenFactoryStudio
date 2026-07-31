#pragma once

#include "formats/FormatsExport.h"
#include "formats/maker/SprMakerDef.h"
#include "formats/snd/SndDocument.h"
#include "formats/util/Result.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace OFS::Formats {

// The list Elecbyte's SndMaker reads from standard input: the SND file to create, then a WAV
// file, group number and sound number per sound, one per line. It ends at an empty line.

struct SndMakerSound
{
    std::string file;
    std::int32_t group = 0;
    std::int32_t number = 0;

    friend bool operator==(const SndMakerSound&, const SndMakerSound&) = default;
};

struct OFS_FORMATS_EXPORT SndMakerScript
{
    std::string outputFile;
    std::vector<SndMakerSound> sounds;

    static Result<SndMakerScript> parse(std::string_view text);
    Result<std::string> write() const;

    friend bool operator==(const SndMakerScript&, const SndMakerScript&) = default;
};

struct SndMakerExportOptions
{
    std::string outputFile = "sounds.snd";
    // Put in front of every WAV name in the list, like "work/kfm/sound/".
    std::string pathPrefix;
    std::vector<std::size_t> sounds;
};

struct SndMakerExport
{
    SndMakerScript script;
    std::vector<MakerFile> files;
};

OFS_FORMATS_EXPORT Result<SndMakerExport> exportSndMaker(const SndDocument& snd,
                                                         const SndMakerExportOptions& options = {});

} // namespace OFS::Formats
