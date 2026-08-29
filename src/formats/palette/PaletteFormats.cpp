#include "formats/palette/PaletteFormats.h"

#include "formats/codecs/Bmp.h"
#include "formats/codecs/Gif.h"
#include "formats/codecs/Pcx.h"
#include "formats/codecs/Png.h"
#include "formats/util/ByteReader.h"
#include "formats/util/ByteWriter.h"

#include <algorithm>
#include <bit>
#include <charconv>
#include <cmath>
#include <optional>
#include <vector>

namespace OFS::Formats {

namespace {

constexpr std::size_t kActColors = 256;
constexpr std::size_t kActSize = kActColors * 3;

std::uint8_t toByte(double v)
{
    return static_cast<std::uint8_t>(std::clamp(std::lround(v * 255.0), 0L, 255L));
}

Rgba fromHsb(double h, double s, double v)
{
    h = std::fmod(h, 360.0) / 60.0;
    const int sector = static_cast<int>(std::floor(h)) % 6;
    const double f = h - std::floor(h);
    const double p = v * (1 - s);
    const double q = v * (1 - s * f);
    const double t = v * (1 - s * (1 - f));
    switch (sector) {
    case 0:
        return rgb(toByte(v), toByte(t), toByte(p));
    case 1:
        return rgb(toByte(q), toByte(v), toByte(p));
    case 2:
        return rgb(toByte(p), toByte(v), toByte(t));
    case 3:
        return rgb(toByte(p), toByte(q), toByte(v));
    case 4:
        return rgb(toByte(t), toByte(p), toByte(v));
    default:
        return rgb(toByte(v), toByte(p), toByte(q));
    }
}

// CIE L*a*b* (D50 white) to sRGB.
Rgba fromLab(double l, double a, double b)
{
    const double fy = (l + 16.0) / 116.0;
    const double fx = fy + a / 500.0;
    const double fz = fy - b / 200.0;
    auto inverse = [](double t) {
        const double t3 = t * t * t;
        return t3 > 0.008856 ? t3 : (t - 16.0 / 116.0) / 7.787;
    };
    const double x = 0.9642 * inverse(fx);
    const double y = 1.0000 * inverse(fy);
    const double z = 0.8249 * inverse(fz);
    // Bradford-adapted D50 XYZ to linear sRGB.
    const double rl = 3.1338561 * x - 1.6168667 * y - 0.4906146 * z;
    const double gl = -0.9787684 * x + 1.9161415 * y + 0.0334540 * z;
    const double bl = 0.0719453 * x - 0.2289914 * y + 1.4052427 * z;
    auto encode = [](double c) {
        c = std::clamp(c, 0.0, 1.0);
        return c <= 0.0031308 ? 12.92 * c : 1.055 * std::pow(c, 1.0 / 2.4) - 0.055;
    };
    return rgb(toByte(encode(rl)), toByte(encode(gl)), toByte(encode(bl)));
}

double unit(float value)
{
    return std::isfinite(value) ? std::clamp(static_cast<double>(value), 0.0, 1.0) : 0.0;
}

double finite(float value)
{
    return std::isfinite(value) ? static_cast<double>(value) : 0.0;
}

void writeUtf16Name(ByteWriter& out, const std::string& text)
{
    for (const char c : text)
        out.u16be(static_cast<std::uint8_t>(c));
    out.u16be(0);
}

std::vector<std::string_view> splitLines(std::string_view text)
{
    std::vector<std::string_view> lines;
    std::size_t start = 0;
    while (start < text.size()) {
        std::size_t end = text.find('\n', start);
        if (end == std::string_view::npos)
            end = text.size();
        std::string_view line = text.substr(start, end - start);
        if (!line.empty() && line.back() == '\r')
            line.remove_suffix(1);
        lines.push_back(line);
        start = end + 1;
    }
    return lines;
}

std::string_view trim(std::string_view s)
{
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t'))
        s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t'))
        s.remove_suffix(1);
    return s;
}

bool parseNumbers(std::string_view line, std::vector<long>& out)
{
    out.clear();
    line = trim(line);
    while (!line.empty()) {
        long value = 0;
        const auto [end, ec] = std::from_chars(line.data(), line.data() + line.size(), value);
        if (ec != std::errc())
            return false;
        out.push_back(value);
        line = trim(line.substr(static_cast<std::size_t>(end - line.data())));
    }
    return true;
}

} // namespace

Result<Palette> readAct(ByteSpan data, ActOrder order)
{
    if (data.size() != kActSize && data.size() != kActSize + 4)
        return Error{"an ACT palette must be 768 or 772 bytes long"};
    const std::span<const std::uint8_t> in = asU8(data);
    Palette palette(kActColors);
    for (std::size_t i = 0; i < kActColors; ++i) {
        const std::size_t index = order == ActOrder::Reversed ? kActColors - 1 - i : i;
        palette[index] = rgb(in[i * 3], in[i * 3 + 1], in[i * 3 + 2]);
    }
    return palette;
}

Bytes writeAct(const Palette& palette, ActOrder order)
{
    ByteWriter out(kActSize);
    for (std::size_t i = 0; i < kActColors; ++i) {
        const std::size_t index = order == ActOrder::Reversed ? kActColors - 1 - i : i;
        const Rgba c = index < palette.size() ? palette[index] : rgb(0, 0, 0);
        out.u8(c.r);
        out.u8(c.g);
        out.u8(c.b);
    }
    return out.take();
}

Result<Palette> readRiffPal(ByteSpan data)
{
    ByteReader r(data);
    if (toText(r.bytes(4)) != "RIFF")
        return Error{"not a RIFF file"};
    const std::uint32_t riffSize = r.u32();
    if (toText(r.bytes(4)) != "PAL ")
        return Error{"not a RIFF palette"};
    if (!r.ok() || riffSize < 4 || riffSize > data.size() - 8)
        return Error{"RIFF palette is truncated"};
    const std::size_t end = 8 + std::size_t{riffSize};
    while (r.position() + 8 <= end) {
        const std::string id = toText(r.bytes(4));
        const std::uint32_t size = r.u32();
        if (size > end - r.position())
            return Error{"RIFF palette chunk is truncated"};
        if (id != "data") {
            r.skip(std::size_t{size} + (size & 1));
            continue;
        }
        if (size < 4)
            return Error{"RIFF palette data chunk is too small"};
        r.u16(); // version, 0x0300
        const std::uint16_t count = r.u16();
        if (std::size_t{count} * 4 > size - 4)
            return Error{"RIFF palette data chunk is truncated"};
        Palette palette(count);
        for (auto& c : palette) {
            c.r = r.u8();
            c.g = r.u8();
            c.b = r.u8();
            r.u8(); // flags
        }
        return palette;
    }
    return Error{"RIFF palette has no data chunk"};
}

Bytes writeRiffPal(const Palette& palette)
{
    const std::size_t count = std::min<std::size_t>(palette.size(), 0xFFFF);
    const auto dataSize = static_cast<std::uint32_t>(4 + count * 4);
    ByteWriter out(12 + 8 + dataSize);
    out.text("RIFF");
    out.u32(4 + 8 + dataSize);
    out.text("PAL ");
    out.text("data");
    out.u32(dataSize);
    out.u16(0x0300);
    out.u16(static_cast<std::uint16_t>(count));
    for (std::size_t i = 0; i < count; ++i) {
        out.u8(palette[i].r);
        out.u8(palette[i].g);
        out.u8(palette[i].b);
        out.u8(0);
    }
    return out.take();
}

Result<Palette> readJascPal(std::string_view text)
{
    const std::vector<std::string_view> lines = splitLines(text);
    if (lines.size() < 3 || trim(lines[0]) != "JASC-PAL")
        return Error{"not a JASC-PAL palette"};
    std::vector<long> numbers;
    if (!parseNumbers(lines[2], numbers) || numbers.size() != 1 || numbers[0] < 0 || numbers[0] > 65536)
        return Error{"JASC-PAL color count is invalid"};
    const auto count = static_cast<std::size_t>(numbers[0]);
    if (lines.size() < 3 + count)
        return Error{"JASC-PAL palette is truncated"};
    Palette palette(count);
    for (std::size_t i = 0; i < count; ++i) {
        if (!parseNumbers(lines[3 + i], numbers) || numbers.size() < 3)
            return Error{"JASC-PAL color " + std::to_string(i) + " is invalid"};
        for (std::size_t k = 0; k < 3; ++k) {
            if (numbers[k] < 0 || numbers[k] > 255)
                return Error{"JASC-PAL color " + std::to_string(i) + " is out of range"};
        }
        palette[i] = rgb(static_cast<std::uint8_t>(numbers[0]), static_cast<std::uint8_t>(numbers[1]),
                         static_cast<std::uint8_t>(numbers[2]));
    }
    return palette;
}

Result<Palette> readPainterSwatches(std::string_view text)
{
    // "R: 255, G: 128, B: 0  HV: 0.08, SV: 1.00, VV: 1.00  "Orange""
    auto component = [](std::string_view line, std::string_view key, long& value) {
        const std::size_t at = line.find(key);
        if (at == std::string_view::npos)
            return false;
        std::size_t i = at + key.size();
        while (i < line.size() && (line[i] == ' ' || line[i] == '\t'))
            ++i;
        const char* first = line.data() + i;
        const char* last = line.data() + line.size();
        const auto [end, ec] = std::from_chars(first, last, value);
        return ec == std::errc() && end != first && value >= 0 && value <= 255;
    };
    Palette palette;
    for (std::string_view line : splitLines(text)) {
        line = trim(line);
        if (line.rfind("R:", 0) != 0)
            continue;
        long r = 0;
        long g = 0;
        long b = 0;
        if (!component(line, "R:", r) || !component(line, "G:", g) || !component(line, "B:", b))
            return Error{"Painter color " + std::to_string(palette.size()) + " is invalid"};
        palette.push_back(
            rgb(static_cast<std::uint8_t>(r), static_cast<std::uint8_t>(g), static_cast<std::uint8_t>(b)));
    }
    if (palette.empty())
        return Error{"not a Painter color set"};
    return palette;
}

std::string writeJascPal(const Palette& palette)
{
    std::string out = "JASC-PAL\r\n0100\r\n" + std::to_string(palette.size()) + "\r\n";
    for (const Rgba& c : palette) {
        out += std::to_string(c.r) + ' ' + std::to_string(c.g) + ' ' + std::to_string(c.b);
        out += "\r\n";
    }
    return out;
}

Result<Palette> readAco(ByteSpan data)
{
    ByteReader r(data);
    Palette best;
    bool any = false;
    while (r.remaining() >= 4) {
        const std::uint16_t version = r.u16be();
        const std::uint16_t count = r.u16be();
        if (version != 1 && version != 2)
            return any ? Result<Palette>(best) : Result<Palette>(Error{"not a Photoshop swatch (.aco) file"});
        Palette palette;
        palette.reserve(count);
        for (std::uint16_t i = 0; i < count; ++i) {
            const std::uint16_t space = r.u16be();
            const std::uint16_t w = r.u16be();
            const std::uint16_t x = r.u16be();
            const std::uint16_t y = r.u16be();
            const std::uint16_t z = r.u16be();
            if (version == 2) {
                const std::uint32_t nameLength = r.u32be();
                if (nameLength > r.remaining() / 2)
                    return Error{"Photoshop swatch name is truncated"};
                r.skip(std::size_t{nameLength} * 2);
            }
            if (!r.ok())
                return Error{"Photoshop swatch file is truncated"};
            switch (space) {
            case 0: // RGB
                palette.push_back(rgb(static_cast<std::uint8_t>(w >> 8), static_cast<std::uint8_t>(x >> 8),
                                      static_cast<std::uint8_t>(y >> 8)));
                break;
            case 1: // HSB
                palette.push_back(fromHsb(w * 360.0 / 65536.0, x / 65535.0, y / 65535.0));
                break;
            case 2: { // CMYK, 0 = full ink
                const double c = 1.0 - w / 65535.0;
                const double m = 1.0 - x / 65535.0;
                const double ye = 1.0 - y / 65535.0;
                const double k = 1.0 - z / 65535.0;
                palette.push_back(
                    rgb(toByte((1 - c) * (1 - k)), toByte((1 - m) * (1 - k)), toByte((1 - ye) * (1 - k))));
                break;
            }
            case 7: // Lab: L 0-10000, a and b signed hundredths
                palette.push_back(
                    fromLab(w / 100.0, static_cast<std::int16_t>(x) / 100.0, static_cast<std::int16_t>(y) / 100.0));
                break;
            case 8: { // grayscale, 0-10000 ink coverage
                const double v = 1.0 - std::min<double>(w, 10000) / 10000.0;
                palette.push_back(rgb(toByte(v), toByte(v), toByte(v)));
                break;
            }
            default:
                break;
            }
        }
        best = std::move(palette);
        any = true;
    }
    if (!any)
        return Error{"Photoshop swatch file is truncated"};
    if (r.remaining() != 0)
        return Error{"Photoshop swatch file has trailing data"};
    return best;
}

Bytes writeAco(const Palette& palette)
{
    const std::size_t count = std::min<std::size_t>(palette.size(), 0xFFFF);
    ByteWriter out;
    for (std::uint16_t version : {std::uint16_t{1}, std::uint16_t{2}}) {
        out.u16be(version);
        out.u16be(static_cast<std::uint16_t>(count));
        for (std::size_t i = 0; i < count; ++i) {
            out.u16be(0); // RGB
            out.u16be(static_cast<std::uint16_t>(palette[i].r * 257));
            out.u16be(static_cast<std::uint16_t>(palette[i].g * 257));
            out.u16be(static_cast<std::uint16_t>(palette[i].b * 257));
            out.u16be(0);
            if (version == 2) {
                const std::string name = std::to_string(i);
                out.u32be(static_cast<std::uint32_t>(name.size() + 1));
                writeUtf16Name(out, name);
            }
        }
    }
    return out.take();
}

Result<Palette> readAse(ByteSpan data)
{
    ByteReader r(data);
    if (toText(r.bytes(4)) != "ASEF")
        return Error{"not an Adobe Swatch Exchange (.ase) file"};
    const std::uint16_t major = r.u16be();
    r.u16be(); // minor version
    const std::uint32_t blocks = r.u32be();
    if (!r.ok())
        return Error{"Adobe Swatch Exchange header is truncated"};
    if (major != 1)
        return Error{"unsupported Adobe Swatch Exchange version " + std::to_string(major)};
    // Every block takes at least 6 bytes (type and length).
    if (blocks > r.remaining() / 6)
        return Error{"Adobe Swatch Exchange file is truncated"};

    Palette palette;
    for (std::uint32_t i = 0; i < blocks; ++i) {
        const std::uint16_t type = r.u16be();
        const std::uint32_t length = r.u32be();
        if (!r.ok() || length > r.remaining())
            return Error{"Adobe Swatch Exchange block is truncated"};
        ByteReader block(r.bytes(length));
        if (type != 0x0001) // group start (0xC001), group end (0xC002) and unknown blocks
            continue;
        const std::uint16_t nameLength = block.u16be(); // UTF-16 code units including the final 0
        block.skip(std::size_t{nameLength} * 2);
        const std::string model = toText(block.bytes(4));
        auto value = [&block] {
            return std::bit_cast<float>(block.u32be());
        };
        std::optional<Rgba> color;
        if (model == "RGB ") {
            const double red = unit(value());
            const double green = unit(value());
            const double blue = unit(value());
            color = rgb(toByte(red), toByte(green), toByte(blue));
        } else if (model == "CMYK") {
            const double c = unit(value());
            const double m = unit(value());
            const double y = unit(value());
            const double k = unit(value());
            color = rgb(toByte((1 - c) * (1 - k)), toByte((1 - m) * (1 - k)), toByte((1 - y) * (1 - k)));
        } else if (model == "LAB ") {
            const double lightness = finite(value()) * 100.0;
            const double a = finite(value());
            const double b = finite(value());
            color = fromLab(lightness, a, b);
        } else if (model == "Gray") {
            const double gray = unit(value());
            color = rgb(toByte(gray), toByte(gray), toByte(gray));
        }
        block.u16be(); // color type: global, spot or normal
        if (!block.ok())
            return Error{"Adobe Swatch Exchange color entry is truncated"};
        if (color)
            palette.push_back(*color);
    }
    return palette;
}

Bytes writeAse(const Palette& palette)
{
    ByteWriter out;
    out.text("ASEF");
    out.u16be(1);
    out.u16be(0);
    out.u32be(static_cast<std::uint32_t>(palette.size()));
    for (std::size_t i = 0; i < palette.size(); ++i) {
        const std::string name = std::to_string(i);
        out.u16be(0x0001);
        out.u32be(static_cast<std::uint32_t>(2 + (name.size() + 1) * 2 + 4 + 12 + 2));
        out.u16be(static_cast<std::uint16_t>(name.size() + 1));
        writeUtf16Name(out, name);
        out.text("RGB ");
        for (const std::uint8_t channel : {palette[i].r, palette[i].g, palette[i].b})
            out.u32be(std::bit_cast<std::uint32_t>(static_cast<float>(channel) / 255.0f));
        out.u16be(2); // normal color
    }
    return out.take();
}

Result<Palette> paletteFromPcx(ByteSpan data)
{
    auto header = readPcxHeader(data);
    if (!header)
        return header.error();
    if (header->planes != 1)
        return Error{"PCX image has no palette (true color)"};
    auto palette = readPcxPalette(data);
    if (!palette)
        return Error{"PCX image has no 256-color palette"};
    return std::move(*palette);
}

Result<Palette> paletteFromPng(ByteSpan data)
{
    auto info = readPngInfo(data);
    if (!info)
        return info.error();
    if (info->colorType != 3)
        return Error{"PNG image is not palette-based"};
    auto image = decodePng(data);
    if (!image)
        return image.error();
    return image->palette();
}

Result<Palette> paletteFromGif(ByteSpan data)
{
    auto gif = readGif(data, DecodeMode::Strict, 1);
    if (!gif)
        return gif.error();
    if (!gif->globalPalette.empty())
        return gif->globalPalette;
    Palette palette = gif->frames.front().image.palette();
    for (Rgba& c : palette)
        c.a = 255;
    return palette;
}

Result<Palette> paletteFromBmp(ByteSpan data)
{
    auto info = readBmpInfo(data);
    if (!info)
        return info.error();
    if (info->bitsPerPixel > 8)
        return Error{"BMP image has no palette (true color)"};
    return std::move(info->palette);
}

Result<Palette> readPaletteFile(ByteSpan data, ActOrder actOrder)
{
    const std::string head = toText(data.first(std::min<std::size_t>(data.size(), 12)));
    const bool actSize = data.size() == kActSize || data.size() == kActSize + 4;
    // An ACT file whose first colors happen to spell an image signature is still an ACT file.
    auto orAct = [&](Result<Palette> result) {
        if (!result && actSize)
            return readAct(data, actOrder);
        return result;
    };
    if (head.rfind("\x89PNG", 0) == 0)
        return orAct(paletteFromPng(data));
    if (head.rfind("GIF8", 0) == 0)
        return orAct(paletteFromGif(data));
    if (head.rfind("BM", 0) == 0)
        return orAct(paletteFromBmp(data));
    if (head.size() >= 12 && head.rfind("RIFF", 0) == 0 && head.substr(8, 4) == "PAL ")
        return readRiffPal(data);
    if (head.rfind("JASC-PAL", 0) == 0)
        return readJascPal(toText(data));
    if (head.rfind("ASEF", 0) == 0)
        return orAct(readAse(data));
    if (head.rfind("ROWS", 0) == 0 || head.rfind("R:", 0) == 0)
        return orAct(readPainterSwatches(toText(data)));
    if (actSize)
        return readAct(data, actOrder);
    if (!data.empty() && static_cast<std::uint8_t>(data[0]) == 0x0A)
        return paletteFromPcx(data);
    return readAco(data);
}

} // namespace OFS::Formats
