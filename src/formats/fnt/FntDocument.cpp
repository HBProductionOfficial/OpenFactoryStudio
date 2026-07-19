#include "formats/fnt/FntDocument.h"

#include "formats/codecs/Pcx.h"
#include "formats/util/ByteReader.h"
#include "formats/util/ByteWriter.h"
#include "formats/util/FileIo.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <string_view>

namespace OFS::Formats {

namespace {

constexpr std::string_view kSignature{"ElecbyteFnt\0", 12};
constexpr std::size_t kHeaderSize = 32;

std::string_view trim(std::string_view s)
{
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
        s.remove_prefix(1);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
        s.remove_suffix(1);
    return s;
}

std::string lower(std::string_view s)
{
    std::string out(s);
    for (char& c : out)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

std::vector<int> numbers(std::string_view value)
{
    std::vector<int> out;
    while (true) {
        value = trim(value);
        int v = 0;
        const auto [end, ec] = std::from_chars(value.data(), value.data() + value.size(), v);
        if (ec != std::errc())
            break;
        out.push_back(v);
        value.remove_prefix(static_cast<std::size_t>(end - value.data()));
        value = trim(value);
        if (value.empty() || value.front() != ',')
            break;
        value.remove_prefix(1);
    }
    return out;
}

std::vector<std::string_view> tokens(std::string_view line)
{
    std::vector<std::string_view> out;
    std::size_t i = 0;
    while (i < line.size()) {
        while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i])))
            ++i;
        const std::size_t start = i;
        while (i < line.size() && !std::isspace(static_cast<unsigned char>(line[i])))
            ++i;
        if (i > start)
            out.push_back(line.substr(start, i - start));
    }
    return out;
}

bool parseInt(std::string_view s, int& out, int base = 10)
{
    const auto [end, ec] = std::from_chars(s.data(), s.data() + s.size(), out, base);
    return ec == std::errc() && end == s.data() + s.size();
}

} // namespace

FntInfo parseFntText(const std::string& text)
{
    FntInfo info;
    std::string section;
    int fixedIndex = 0;
    std::size_t start = 0;
    while (start <= text.size()) {
        std::size_t end = text.find('\n', start);
        if (end == std::string::npos)
            end = text.size();
        std::string_view line(text.data() + start, end - start);
        start = end + 1;
        if (!line.empty() && line.back() == '\r')
            line.remove_suffix(1);

        const std::string_view trimmed = trim(line);
        if (!trimmed.empty() && trimmed.front() == '[') {
            const std::size_t close = trimmed.find(']');
            section = lower(trimmed.substr(1, close == std::string_view::npos ? std::string_view::npos : close - 1));
            continue;
        }

        if (section == "map") {
            // The character itself may be ';' or '=', so comments are only stripped after it.
            std::vector<std::string_view> parts = tokens(line);
            if (parts.empty())
                continue;
            int code = -1;
            std::string_view first = parts[0];
            if (first.size() == 1) {
                code = static_cast<unsigned char>(first[0]);
            } else if (first.size() > 2 && first[0] == '0' && (first[1] == 'x' || first[1] == 'X')) {
                if (!parseInt(first.substr(2), code, 16))
                    code = -1;
            }
            if (code < 0 || code > 255)
                continue;
            std::vector<int> values;
            for (std::size_t k = 1; k < parts.size(); ++k) {
                if (parts[k].front() == ';')
                    break;
                int v = 0;
                if (!parseInt(parts[k], v))
                    break;
                values.push_back(v);
            }
            FntInfo::Glyph glyph;
            glyph.code = static_cast<std::uint8_t>(code);
            if (info.type == FntInfo::Type::Variable) {
                if (values.size() < 2)
                    continue;
                glyph.x = values[0];
                glyph.width = values[1];
            } else {
                glyph.x = values.empty() ? fixedIndex : values[0];
                glyph.width = info.width;
                fixedIndex = glyph.x + 1;
            }
            info.glyphs.push_back(glyph);
            continue;
        }

        if (section != "def")
            continue;
        std::string_view content = line;
        if (const std::size_t semicolon = content.find(';'); semicolon != std::string_view::npos)
            content = content.substr(0, semicolon);
        const std::size_t eq = content.find('=');
        if (eq == std::string_view::npos)
            continue;
        const std::string key = lower(trim(content.substr(0, eq)));
        const std::string_view value = trim(content.substr(eq + 1));
        const std::vector<int> v = numbers(value);
        if (key == "size" && v.size() >= 2) {
            info.width = v[0];
            info.height = v[1];
        } else if (key == "spacing" && v.size() >= 2) {
            info.spacingX = v[0];
            info.spacingY = v[1];
        } else if (key == "colors" && !v.empty()) {
            info.colors = v[0];
        } else if (key == "offset" && v.size() >= 2) {
            info.offsetX = v[0];
            info.offsetY = v[1];
        } else if (key == "type") {
            info.type = lower(value) == "fixed" ? FntInfo::Type::Fixed : FntInfo::Type::Variable;
        }
    }
    return info;
}

Result<FntDocument> FntDocument::read(ByteSpan data)
{
    if (data.size() < kHeaderSize)
        return Error{"FNT header is truncated"};
    if (toText(data.first(kSignature.size())) != kSignature)
        return Error{"not an FNT v1 file (signature missing)"};
    ByteReader r(data);
    r.seek(12);
    FntDocument doc;
    for (std::uint8_t& b : doc.versionBytes)
        b = r.u8();
    const std::uint32_t pcxOffset = r.u32();
    const std::uint32_t pcxSize = r.u32();
    const std::uint32_t textOffset = r.u32();
    const std::uint32_t textSize = r.u32();
    auto inside = [&](std::uint64_t offset, std::uint64_t size) {
        return offset <= data.size() && size <= data.size() - offset;
    };
    if (pcxOffset < kHeaderSize || !inside(pcxOffset, pcxSize))
        return Error{"FNT image lies outside the file"};
    if (textOffset < kHeaderSize || !inside(textOffset, textSize))
        return Error{"FNT text lies outside the file"};
    const ByteSpan padding = data.subspan(kHeaderSize, pcxOffset - kHeaderSize);
    doc.headerPadding.assign(padding.begin(), padding.end());
    const ByteSpan image = data.subspan(pcxOffset, pcxSize);
    doc.pcx.assign(image.begin(), image.end());
    doc.text = toText(data.subspan(textOffset, textSize));
    return doc;
}

Result<FntDocument> FntDocument::load(const std::filesystem::path& path)
{
    auto data = readFile(path);
    if (!data)
        return data.error();
    return read(*data);
}

Bytes FntDocument::write() const
{
    ByteWriter out(kHeaderSize + headerPadding.size() + pcx.size() + text.size());
    out.text(kSignature);
    for (const std::uint8_t b : versionBytes)
        out.u8(b);
    const std::size_t pcxOffset = kHeaderSize + headerPadding.size();
    out.u32(static_cast<std::uint32_t>(pcxOffset));
    out.u32(static_cast<std::uint32_t>(pcx.size()));
    out.u32(static_cast<std::uint32_t>(pcxOffset + pcx.size()));
    out.u32(static_cast<std::uint32_t>(text.size()));
    out.bytes(headerPadding);
    out.bytes(pcx);
    out.text(text);
    return out.take();
}

Status FntDocument::save(const std::filesystem::path& path) const
{
    return writeFile(path, write());
}

Result<Image> FntDocument::image() const
{
    return decodePcx(pcx, PcxPalette::Optional, DecodeMode::Lenient);
}

} // namespace OFS::Formats
