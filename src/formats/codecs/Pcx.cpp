#include "formats/codecs/Pcx.h"

#include "formats/util/ByteReader.h"
#include "formats/util/ByteWriter.h"

#include <algorithm>
#include <string>

namespace OFS::Formats {

namespace {

constexpr std::uint8_t kManufacturer = 0x0A;
// So damaged headers can't ask for huge buffers.
constexpr std::size_t kMaxDecodeBytes = std::size_t{1} << 27;
constexpr std::uint8_t kPaletteMarker = 0x0C;
constexpr std::size_t kMaxRun = 0x3F;

bool unpackRle(std::span<const std::uint8_t> in, std::size_t limit, bool rle, std::vector<std::uint8_t>& out,
               bool strict, bool& overflow)
{
    std::size_t pos = kPcxHeaderSize;
    std::size_t produced = 0;
    const std::size_t total = out.size();
    if (!rle) {
        const std::size_t available = limit > pos ? limit - pos : 0;
        const std::size_t n = std::min(available, total);
        std::copy_n(in.begin() + static_cast<std::ptrdiff_t>(pos), n, out.begin());
        return n == total;
    }
    while (produced < total) {
        if (pos >= limit)
            return false;
        const std::uint8_t b = in[pos++];
        std::size_t count = 1;
        std::uint8_t value = b;
        if ((b & 0xC0) == 0xC0) {
            if (pos >= limit)
                return false;
            count = b & kMaxRun;
            value = in[pos++];
        }
        if (count > total - produced) {
            if (strict) {
                overflow = true;
                return false;
            }
            count = total - produced;
        }
        std::fill_n(out.begin() + static_cast<std::ptrdiff_t>(produced), count, value);
        produced += count;
    }
    return true;
}

Palette paletteAt(std::span<const std::uint8_t> in, std::size_t at)
{
    Palette palette(256);
    for (std::size_t i = 0; i < 256; ++i)
        palette[i] = rgb(in[at + i * 3], in[at + i * 3 + 1], in[at + i * 3 + 2]);
    return palette;
}

void encodeScanline(ByteWriter& out, std::span<const std::uint8_t> line)
{
    std::size_t i = 0;
    while (i < line.size()) {
        const std::uint8_t value = line[i];
        std::size_t run = 1;
        while (i + run < line.size() && run < kMaxRun && line[i + run] == value)
            ++run;
        if (run == 1 && (value & 0xC0) != 0xC0) {
            out.u8(value);
        } else {
            out.u8(static_cast<std::uint8_t>(0xC0 | run));
            out.u8(value);
        }
        i += run;
    }
}

} // namespace

Result<PcxHeader> readPcxHeader(ByteSpan data)
{
    if (data.size() < kPcxHeaderSize)
        return Error{"PCX header is truncated"};
    ByteReader r(data);
    if (r.u8() != kManufacturer)
        return Error{"not a PCX image"};
    PcxHeader h;
    h.version = r.u8();
    h.encoding = r.u8();
    h.bitsPerPixel = r.u8();
    h.xMin = r.u16();
    h.yMin = r.u16();
    h.xMax = r.u16();
    h.yMax = r.u16();
    h.hDpi = r.u16();
    h.vDpi = r.u16();
    r.seek(65);
    h.planes = r.u8();
    h.bytesPerLine = r.u16();
    h.paletteInfo = r.u16();
    if (h.encoding > 1)
        return Error{"unknown PCX encoding " + std::to_string(h.encoding)};
    if (h.bitsPerPixel != 8)
        return Error{"unsupported PCX bit depth " + std::to_string(h.bitsPerPixel)};
    if (h.planes != 1 && h.planes != 3 && h.planes != 4)
        return Error{"unsupported PCX plane count " + std::to_string(h.planes)};
    if (h.xMax < h.xMin || h.yMax < h.yMin)
        return Error{"invalid PCX image size"};
    if (h.bytesPerLine < h.width())
        return Error{"PCX bytes per line is smaller than the width"};
    return h;
}

std::optional<Palette> readPcxPalette(ByteSpan data)
{
    if (data.size() < kPcxHeaderSize + kPcxPaletteSize)
        return std::nullopt;
    const std::span<const std::uint8_t> in = asU8(data);
    const std::size_t marker = in.size() - kPcxPaletteSize;
    if (in[marker] != kPaletteMarker)
        return std::nullopt;
    return paletteAt(in, marker + 1);
}

Result<Image> decodePcx(ByteSpan data, PcxPalette palette, DecodeMode mode)
{
    auto header = readPcxHeader(data);
    if (!header)
        return header.error();
    const PcxHeader& h = *header;
    const bool strict = mode == DecodeMode::Strict;
    const std::span<const std::uint8_t> in = asU8(data);
    const int width = h.width();
    const int height = h.height();
    const std::size_t stride = std::size_t{h.bytesPerLine} * h.planes;
    const std::size_t total = stride * static_cast<std::size_t>(height);
    if (total > kMaxDecodeBytes)
        return Error{"PCX image is too large to decode"};
    // A run of up to 63 bytes takes two input bytes, so data that is far too short fails early.
    if (strict && (h.encoding == 1 ? (in.size() - kPcxHeaderSize) * 32 : in.size() - kPcxHeaderSize) < total)
        return Error{"PCX image data is truncated"};
    std::vector<std::uint8_t> raw(total, 0);

    bool overflow = false;
    bool decoded = false;
    std::optional<Palette> colors;
    if (h.planes == 1) {
        // The image data must end before a trailing palette.
        if (in.size() >= kPcxHeaderSize + kPcxPaletteSize && in[in.size() - kPcxPaletteSize] == kPaletteMarker) {
            decoded = unpackRle(in, in.size() - kPcxPaletteSize, h.encoding == 1, raw, strict, overflow);
            if (decoded || !strict)
                colors = paletteAt(in, in.size() - kPcxPaletteSize + 1);
        }
        if (!colors && !overflow) {
            if (palette == PcxPalette::Required)
                return Error{"PCX image data is truncated or the 256-color palette is missing"};
            std::fill(raw.begin(), raw.end(), std::uint8_t{0});
            decoded = unpackRle(in, in.size(), h.encoding == 1, raw, strict, overflow);
        }
    } else {
        decoded = unpackRle(in, in.size(), h.encoding == 1, raw, strict, overflow);
    }
    if (overflow)
        return Error{"PCX run goes past the end of the image"};
    if (!decoded && strict)
        return Error{"PCX image data is truncated"};

    if (h.planes == 1) {
        std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
        for (int y = 0; y < height; ++y)
            std::copy_n(raw.begin() + static_cast<std::ptrdiff_t>(static_cast<std::size_t>(y) * stride),
                        static_cast<std::size_t>(width),
                        pixels.begin() +
                            static_cast<std::ptrdiff_t>(static_cast<std::size_t>(y) * static_cast<std::size_t>(width)));
        return Image::indexed(width, height, std::move(pixels), colors ? std::move(*colors) : Palette{});
    }

    Image image(width, height, PixelFormat::Rgba32);
    const std::size_t bpl = h.bytesPerLine;
    for (int y = 0; y < height; ++y) {
        const std::uint8_t* line = raw.data() + static_cast<std::size_t>(y) * stride;
        for (int x = 0; x < width; ++x) {
            const auto ux = static_cast<std::size_t>(x);
            const std::uint8_t alpha = h.planes == 4 ? line[3 * bpl + ux] : std::uint8_t{255};
            image.setColor(x, y, rgb(line[ux], line[bpl + ux], line[2 * bpl + ux], alpha));
        }
    }
    return image;
}

Result<Bytes> encodePcx(const Image& image, const PcxEncodeOptions& options)
{
    if (!image.isIndexed())
        return Error{"PCX encoding needs an indexed image"};
    if (image.isNull() || image.width() > 0xFFFF || image.height() > 0xFFFF)
        return Error{"PCX images must be between 1x1 and 65535x65535 pixels"};
    const auto width = static_cast<std::size_t>(image.width());
    const std::size_t bytesPerLine = width + (width & 1);

    ByteWriter out(kPcxHeaderSize + image.pixelCount() + kPcxPaletteSize);
    out.u8(kManufacturer);
    out.u8(5); // version 3.0 with palette
    out.u8(1); // RLE
    out.u8(8); // bits per pixel per plane
    out.u16(0);
    out.u16(0);
    out.u16(static_cast<std::uint16_t>(image.width() - 1));
    out.u16(static_cast<std::uint16_t>(image.height() - 1));
    out.u16(options.dpi);
    out.u16(options.dpi);
    out.zeros(48); // 16-color EGA palette, unused
    out.u8(0);     // reserved
    out.u8(1);     // planes
    out.u16(static_cast<std::uint16_t>(bytesPerLine));
    out.u16(1); // color palette
    out.u16(0);
    out.u16(0);
    out.padTo(kPcxHeaderSize);

    std::vector<std::uint8_t> line(bytesPerLine, 0);
    const std::span<const std::uint8_t> pixels = image.pixels();
    for (std::size_t y = 0; y < static_cast<std::size_t>(image.height()); ++y) {
        std::copy_n(pixels.begin() + static_cast<std::ptrdiff_t>(y * width), width, line.begin());
        encodeScanline(out, line);
    }

    if (options.includePalette) {
        out.u8(kPaletteMarker);
        const Palette& palette = image.palette();
        for (std::size_t i = 0; i < 256; ++i) {
            const Rgba c = i < palette.size() ? palette[i] : rgb(0, 0, 0);
            out.u8(c.r);
            out.u8(c.g);
            out.u8(c.b);
        }
    }
    return out.take();
}

} // namespace OFS::Formats
