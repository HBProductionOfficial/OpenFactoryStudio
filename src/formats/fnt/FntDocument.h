#pragma once

#include "formats/FormatsExport.h"
#include "formats/image/Image.h"
#include "formats/util/Bytes.h"
#include "formats/util/Result.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace OFS::Formats {

struct FntInfo
{
    enum class Type : std::uint8_t
    {
        Fixed,
        Variable,
    };

    struct Glyph
    {
        std::uint8_t code = 0;
        int x = 0; // left edge in the image (Variable) or glyph index (Fixed)
        int width = 0;
    };

    int width = 0;
    int height = 0;
    int spacingX = 0;
    int spacingY = 0;
    int colors = 0;
    int offsetX = 0;
    int offsetY = 0;
    Type type = Type::Variable;
    std::vector<Glyph> glyphs;
};

// A map line is the character or a 0xNN code, then "x width" for variable fonts or an optional
// index for fixed fonts.
OFS_FORMATS_EXPORT FntInfo parseFntText(const std::string& text);

// Elecbyte FNT v1 font: a PCX with the glyphs side by side and a text block that maps them.
class OFS_FORMATS_EXPORT FntDocument
{
public:
    std::array<std::uint8_t, 4> versionBytes = {0, 1, 0, 0};
    Bytes pcx;
    std::string text;
    // Some files have a few bytes between the 32-byte header and the PCX.
    Bytes headerPadding;

    static Result<FntDocument> read(ByteSpan data);
    static Result<FntDocument> load(const std::filesystem::path& path);
    Bytes write() const;
    Status save(const std::filesystem::path& path) const;

    Result<Image> image() const;
    FntInfo info() const { return parseFntText(text); }
};

} // namespace OFS::Formats
