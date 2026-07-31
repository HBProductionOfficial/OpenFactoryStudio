#pragma once

#include "formats/FormatsExport.h"
#include "formats/sff/SffDocument.h"
#include "formats/util/Bytes.h"
#include "formats/util/Result.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace OFS::Formats {

// The .def that Elecbyte's SprMaker2 builds SFF v2.0 files from:
//   [Output]
//   filename = chars/kfm/kfm.sff
//   [Pal]
//   1,1, kfm.act, 0,255          ; group, number, file, first color, last color
//   [Option]
//   sprite.usepal = 1,1          ; applies to the sprites after it
//   [Sprite]
//   0, 0, stand00.png, 18, 105   ; group, number, file, axis x, axis y [? usepal = ... ? crop = ...]

struct SprMakerId
{
    std::int32_t group = 0;
    std::int32_t number = 0;

    friend bool operator==(const SprMakerId&, const SprMakerId&) = default;
};

enum class SprMakerCompression : std::uint8_t
{
    None,
    Rle8,
    Rle5,
    Lz5,
};

OFS_FORMATS_EXPORT std::string_view sprMakerCompressionName(SprMakerCompression compression);

struct SprMakerColorRange
{
    int first = 0;
    int last = 0;

    friend bool operator==(const SprMakerColorRange&, const SprMakerColorRange&) = default;
};

struct SprMakerCrop
{
    int x1 = 0;
    int y1 = 0;
    int x2 = 0;
    int y2 = 0;

    friend bool operator==(const SprMakerCrop&, const SprMakerCrop&) = default;
};

struct SprMakerSpriteOptions
{
    SprMakerCompression compress5 = SprMakerCompression::None;
    SprMakerCompression compress8 = SprMakerCompression::None;
    SprMakerCompression compress24 = SprMakerCompression::None;
    bool decompressOnLoad = false;
    bool detectDuplicates = false;
    bool autoCrop = false;
    // Empty means -1: the image's own palette, added under the sprite's group and number.
    std::optional<SprMakerId> usePalette;
    // 32,255 makes a 256-color image a 5-bit sprite. Empty means -1.
    std::optional<SprMakerColorRange> removeColors;

    friend bool operator==(const SprMakerSpriteOptions&, const SprMakerSpriteOptions&) = default;
};

struct SprMakerPalette
{
    SprMakerId id;
    std::string file;
    int firstColor = 0;
    int lastColor = 255;

    friend bool operator==(const SprMakerPalette&, const SprMakerPalette&) = default;
};

struct SprMakerSprite
{
    SprMakerId id;
    std::string file;
    std::int32_t axisX = 0;
    std::int32_t axisY = 0;
    SprMakerSpriteOptions options;
    std::optional<SprMakerCrop> crop;

    friend bool operator==(const SprMakerSprite&, const SprMakerSprite&) = default;
};

// Each sprite keeps the switches in effect for it. Writing groups runs of equal switches under
// one [Option] section, so a written file reads back to the same model.
struct OFS_FORMATS_EXPORT SprMakerDef
{
    std::string outputFile;
    std::string inputDir;
    bool palDetectDuplicates = false;
    bool palDiscardDuplicates = false;
    bool palReverseAct = false; // 1: natural order, 0: MUGEN order
    bool palReversePng = false;
    std::vector<SprMakerPalette> palettes;
    std::vector<SprMakerSprite> sprites;

    static Result<SprMakerDef> parse(std::string_view text);
    Result<std::string> write() const;

    friend bool operator==(const SprMakerDef&, const SprMakerDef&) = default;
};

struct MakerFile
{
    std::string name;
    Bytes data;
};

struct SprMakerExportOptions
{
    std::string outputFile = "sprites.sff";
    SprMakerCompression compress5 = SprMakerCompression::Lz5;
    SprMakerCompression compress8 = SprMakerCompression::Rle8;
    std::vector<std::size_t> sprites;
};

struct SprMakerExport
{
    SprMakerDef def;
    std::vector<MakerFile> files;
    std::vector<std::string> warnings;
};

// Builds a project that sprmake2 turns back into the same SFF. Palettes are padded to 32 or 256
// colors, the only sizes SprMaker2 makes. True-color sprites and sprites with no palette are
// skipped with a warning.
OFS_FORMATS_EXPORT Result<SprMakerExport> exportSprMaker(const SffDocument& sff,
                                                         const SprMakerExportOptions& options = {});

} // namespace OFS::Formats
