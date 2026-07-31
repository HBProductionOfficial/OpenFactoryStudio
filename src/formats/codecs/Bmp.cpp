#include "formats/codecs/Bmp.h"

#include "formats/util/ByteReader.h"
#include "formats/util/ByteWriter.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <string>

namespace OFS::Formats {

namespace {

constexpr std::size_t kFileHeaderSize = 14;
constexpr std::uint32_t kCoreHeaderSize = 12;
constexpr std::uint32_t kInfoHeaderSize = 40;
constexpr std::uint32_t kV4HeaderSize = 108;
// Refuse damaged headers that would need huge buffers.
constexpr std::uint64_t kMaxPixels = std::uint64_t{1} << 26;

bool knownHeaderSize(std::uint32_t size)
{
    return size == kCoreHeaderSize || size == kInfoHeaderSize || size == 52 || size == 56 || size == kV4HeaderSize ||
           size == 124;
}

std::size_t strideOf(int width, int bitsPerPixel)
{
    return ((static_cast<std::size_t>(width) * static_cast<std::size_t>(bitsPerPixel) + 31) / 32) * 4;
}

class Channel
{
public:
    explicit Channel(std::uint32_t mask)
        : m_mask(mask)
    {
        if (mask == 0)
            return;
        m_shift = std::countr_zero(mask);
        m_bits = std::min(std::popcount(mask >> m_shift), 32 - m_shift);
        m_max = m_bits >= 32 ? 0xFFFFFFFFu : (std::uint32_t{1} << m_bits) - 1;
    }

    bool present() const noexcept { return m_mask != 0; }

    std::uint8_t operator()(std::uint32_t pixel) const noexcept
    {
        if (m_mask == 0)
            return 0;
        const std::uint32_t value = (pixel & m_mask) >> m_shift;
        if (m_bits >= 8)
            return static_cast<std::uint8_t>(value >> (m_bits - 8));
        return static_cast<std::uint8_t>((value * 255 + m_max / 2) / m_max);
    }

private:
    std::uint32_t m_mask = 0;
    int m_shift = 0;
    int m_bits = 0;
    std::uint32_t m_max = 1;
};

bool unpackRle(std::span<const std::uint8_t> in, int width, int height, bool four, std::vector<std::uint8_t>& out,
               bool strict, std::string& error)
{
    std::size_t pos = 0;
    long long x = 0;
    long long y = 0;
    auto put = [&](std::uint8_t value) {
        if (x < width && y < height)
            out[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)] = value;
        ++x;
    };
    auto outside = [&](long long count) {
        return y >= height || x + count > width;
    };
    for (;;) {
        if (pos + 2 > in.size()) {
            if (strict) {
                error = "BMP RLE data ends without an end-of-bitmap marker";
                return false;
            }
            return true;
        }
        const std::uint8_t count = in[pos];
        const std::uint8_t value = in[pos + 1];
        pos += 2;
        if (count > 0) {
            if (outside(count) && strict) {
                error = "BMP RLE run goes past the end of a row";
                return false;
            }
            for (int k = 0; k < count; ++k)
                put(four ? static_cast<std::uint8_t>((k & 1) ? (value & 0x0F) : (value >> 4)) : value);
            continue;
        }
        switch (value) {
        case 0: // end of line
            x = 0;
            ++y;
            break;
        case 1: // end of bitmap
            return true;
        case 2: { // delta
            if (pos + 2 > in.size()) {
                if (strict) {
                    error = "BMP RLE delta is truncated";
                    return false;
                }
                return true;
            }
            x += in[pos];
            y += in[pos + 1];
            pos += 2;
            if ((x > width || y > height) && strict) {
                error = "BMP RLE delta leaves the image";
                return false;
            }
            break;
        }
        default: { // absolute run of `value` pixels, padded to a 16-bit boundary
            const std::size_t bytes = four ? (std::size_t{value} + 1) / 2 : value;
            const std::size_t padded = bytes + (bytes & 1);
            if (pos + padded > in.size() || outside(value)) {
                if (strict) {
                    error = pos + padded > in.size() ? "BMP RLE data is truncated"
                                                     : "BMP RLE run goes past the end of a row";
                    return false;
                }
                if (pos + padded > in.size())
                    return true;
            }
            for (int k = 0; k < value; ++k) {
                const std::uint8_t b = in[pos + static_cast<std::size_t>(four ? k / 2 : k)];
                put(four ? static_cast<std::uint8_t>((k & 1) ? (b & 0x0F) : (b >> 4)) : b);
            }
            pos += padded;
            break;
        }
        }
    }
}

std::int32_t pixelsPerMeter(std::uint16_t dpi)
{
    return static_cast<std::int32_t>(std::lround(dpi / 0.0254));
}

} // namespace

Result<BmpInfo> readBmpInfo(ByteSpan data)
{
    ByteReader r(data);
    if (toText(r.bytes(2)) != "BM")
        return Error{"not a BMP image"};
    r.u32(); // file size, often wrong
    r.u32(); // reserved
    BmpInfo info;
    info.dataOffset = r.u32();
    info.headerSize = r.u32();
    if (!r.ok())
        return Error{"BMP header is truncated"};
    if (!knownHeaderSize(info.headerSize))
        return Error{"unsupported BMP header size " + std::to_string(info.headerSize)};
    if (!r.canRead(info.headerSize - 4))
        return Error{"BMP header is truncated"};

    std::uint32_t colorsUsed = 0;
    std::size_t entrySize = 4;
    if (info.headerSize == kCoreHeaderSize) {
        info.width = r.u16();
        info.height = r.u16();
        r.u16(); // planes
        info.bitsPerPixel = r.u16();
        entrySize = 3;
    } else {
        const std::int32_t width = r.s32();
        const std::int32_t height = r.s32();
        r.u16(); // planes
        info.bitsPerPixel = r.u16();
        const std::uint32_t compression = r.u32();
        r.u32(); // image size
        r.s32(); // horizontal resolution
        r.s32(); // vertical resolution
        colorsUsed = r.u32();
        r.u32(); // important colors
        if (width <= 0 || height == 0 || height == std::numeric_limits<std::int32_t>::min())
            return Error{"invalid BMP image size"};
        info.width = width;
        info.height = height < 0 ? -height : height;
        info.topDown = height < 0;
        if (compression > 6)
            return Error{"unknown BMP compression " + std::to_string(compression)};
        info.compression = static_cast<BmpCompression>(compression);
        if (info.headerSize >= 52) {
            info.redMask = r.u32();
            info.greenMask = r.u32();
            info.blueMask = r.u32();
            if (info.headerSize >= 56)
                info.alphaMask = r.u32();
        }
        r.seek(kFileHeaderSize + info.headerSize);
        // A BITMAPINFOHEADER is followed by the masks when the compression asks for them.
        if (info.headerSize == kInfoHeaderSize &&
            (info.compression == BmpCompression::Bitfields || info.compression == BmpCompression::AlphaBitfields)) {
            info.redMask = r.u32();
            info.greenMask = r.u32();
            info.blueMask = r.u32();
            if (info.compression == BmpCompression::AlphaBitfields)
                info.alphaMask = r.u32();
        }
    }
    if (info.width <= 0 || info.height <= 0)
        return Error{"invalid BMP image size"};
    if (std::uint64_t(info.width) * std::uint64_t(info.height) > kMaxPixels)
        return Error{"BMP image is too large to decode"};

    const int bpp = info.bitsPerPixel;
    switch (info.compression) {
    case BmpCompression::Rgb:
        if (bpp != 1 && bpp != 4 && bpp != 8 && bpp != 16 && bpp != 24 && bpp != 32)
            return Error{"unsupported BMP bit depth " + std::to_string(bpp)};
        break;
    case BmpCompression::Rle8:
    case BmpCompression::Rle4:
        if (bpp != (info.compression == BmpCompression::Rle8 ? 8 : 4))
            return Error{"BMP RLE compression does not match the bit depth"};
        if (info.topDown)
            return Error{"BMP RLE images cannot be stored top-down"};
        break;
    case BmpCompression::Bitfields:
    case BmpCompression::AlphaBitfields:
        if (bpp != 16 && bpp != 32)
            return Error{"BMP bit fields need 16 or 32 bits per pixel"};
        break;
    case BmpCompression::Jpeg:
    case BmpCompression::Png:
        return Error{"BMP files with embedded JPEG or PNG data are not supported"};
    }
    if (info.compression == BmpCompression::Rgb && (bpp == 16 || bpp == 32)) {
        // Uncompressed 16 and 32-bit images use fixed layouts whatever the header says.
        info.redMask = bpp == 16 ? 0x7C00 : 0x00FF0000;
        info.greenMask = bpp == 16 ? 0x03E0 : 0x0000FF00;
        info.blueMask = bpp == 16 ? 0x001F : 0x000000FF;
        info.alphaMask = bpp == 16 ? 0 : 0xFF000000;
    }
    if (!r.ok())
        return Error{"BMP header is truncated"};

    std::size_t entries = colorsUsed;
    if (bpp <= 8) {
        const std::size_t maximum = std::size_t{1} << bpp;
        entries = (entries == 0 || entries > maximum) ? maximum : entries;
        if (info.headerSize == kCoreHeaderSize && info.dataOffset > r.position())
            entries = std::min(entries, (info.dataOffset - r.position()) / 3);
    } else {
        entries = std::min<std::size_t>(entries, 256);
    }
    if (!r.canRead(entries * entrySize))
        return Error{"BMP color table is truncated"};
    info.palette.resize(entries);
    for (Rgba& c : info.palette) {
        c.b = r.u8();
        c.g = r.u8();
        c.r = r.u8();
        if (entrySize == 4)
            r.u8();
    }
    if (info.dataOffset > data.size())
        return Error{"BMP pixel data offset is past the end of the file"};
    return info;
}

Result<Image> decodeBmp(ByteSpan data, DecodeMode mode)
{
    auto header = readBmpInfo(data);
    if (!header)
        return header.error();
    const BmpInfo& info = *header;
    const bool strict = mode == DecodeMode::Strict;
    const std::span<const std::uint8_t> in = asU8(data).subspan(info.dataOffset);
    const int width = info.width;
    const int height = info.height;
    const auto w = static_cast<std::size_t>(width);
    const int bpp = info.bitsPerPixel;

    auto rowOf = [&](int fileRow) {
        return info.topDown ? fileRow : height - 1 - fileRow;
    };

    if (info.compression == BmpCompression::Rle8 || info.compression == BmpCompression::Rle4) {
        std::vector<std::uint8_t> bottomUp(w * static_cast<std::size_t>(height), 0);
        std::string error;
        if (!unpackRle(in, width, height, info.compression == BmpCompression::Rle4, bottomUp, strict, error))
            return Error{error};
        std::vector<std::uint8_t> pixels(bottomUp.size());
        for (int y = 0; y < height; ++y)
            std::copy_n(bottomUp.begin() + static_cast<std::ptrdiff_t>(static_cast<std::size_t>(y) * w), w,
                        pixels.begin() + static_cast<std::ptrdiff_t>(static_cast<std::size_t>(rowOf(y)) * w));
        return Image::indexed(width, height, std::move(pixels), info.palette);
    }

    const std::size_t stride = strideOf(width, bpp);
    const std::size_t needed = stride * static_cast<std::size_t>(height);
    if (in.size() < needed && strict)
        return Error{"BMP pixel data is truncated"};
    // Some writers leave out the padding of the last row.
    const auto rows = static_cast<int>(std::min<std::size_t>((in.size() + stride - 1) / stride, std::size_t(height)));
    const auto bytesPerPixel = static_cast<std::size_t>(bpp / 8);
    auto rowPixels = [&](int y) {
        const std::size_t available = std::min(stride, in.size() - static_cast<std::size_t>(y) * stride);
        return std::min(w, bpp <= 8 ? available * 8 / static_cast<std::size_t>(bpp) : available / bytesPerPixel);
    };

    if (bpp <= 8) {
        std::vector<std::uint8_t> pixels(w * static_cast<std::size_t>(height), 0);
        const unsigned mask = (1u << bpp) - 1;
        for (int y = 0; y < rows; ++y) {
            const std::uint8_t* line = in.data() + static_cast<std::size_t>(y) * stride;
            std::uint8_t* dst = pixels.data() + static_cast<std::size_t>(rowOf(y)) * w;
            const std::size_t count = rowPixels(y);
            for (std::size_t x = 0; x < count; ++x) {
                const std::size_t bit = x * static_cast<std::size_t>(bpp);
                const unsigned shift = 8 - static_cast<unsigned>(bpp) - static_cast<unsigned>(bit % 8);
                dst[x] = static_cast<std::uint8_t>((line[bit / 8] >> shift) & mask);
            }
        }
        return Image::indexed(width, height, std::move(pixels), info.palette);
    }

    Image image(width, height, PixelFormat::Rgba32);
    const Channel red(info.redMask);
    const Channel green(info.greenMask);
    const Channel blue(info.blueMask);
    const Channel alpha(info.alphaMask);
    bool anyAlpha = false;
    std::uint8_t* out = image.pixels().data();
    for (int y = 0; y < rows; ++y) {
        const std::uint8_t* line = in.data() + static_cast<std::size_t>(y) * stride;
        std::uint8_t* dst = out + static_cast<std::size_t>(rowOf(y)) * w * 4;
        const std::size_t count = rowPixels(y);
        for (std::size_t x = 0; x < count; ++x, dst += 4) {
            if (bpp == 24) {
                dst[0] = line[x * 3 + 2];
                dst[1] = line[x * 3 + 1];
                dst[2] = line[x * 3];
                dst[3] = 255;
                continue;
            }
            std::uint32_t value = 0;
            if (bpp == 16) {
                value = std::uint32_t{line[x * 2]} | (std::uint32_t{line[x * 2 + 1]} << 8);
            } else {
                value = std::uint32_t{line[x * 4]} | (std::uint32_t{line[x * 4 + 1]} << 8) |
                        (std::uint32_t{line[x * 4 + 2]} << 16) | (std::uint32_t{line[x * 4 + 3]} << 24);
            }
            dst[0] = red(value);
            dst[1] = green(value);
            dst[2] = blue(value);
            dst[3] = alpha.present() ? alpha(value) : 255;
            anyAlpha = anyAlpha || (alpha.present() && dst[3] != 0);
        }
    }
    if (alpha.present() && !anyAlpha) {
        // The alpha byte is unused, so decoded pixels are opaque. Missing ones stay transparent.
        for (int y = 0; y < rows; ++y) {
            std::uint8_t* dst = out + static_cast<std::size_t>(rowOf(y)) * w * 4;
            for (std::size_t x = 0, count = rowPixels(y); x < count; ++x)
                dst[x * 4 + 3] = 255;
        }
    }
    return image;
}

Result<Bytes> encodeBmp(const Image& image, const BmpEncodeOptions& options)
{
    if (image.isNull())
        return Error{"cannot encode an empty image as BMP"};
    if (image.width() > 0xFFFF || image.height() > 0xFFFF)
        return Error{"BMP images must be at most 65535x65535 pixels"};

    const bool indexed = image.isIndexed() && options.color == BmpColor::Automatic;
    const bool withAlpha = !indexed && options.color != BmpColor::Rgb;
    const int bpp = indexed ? 8 : (withAlpha ? 32 : 24);
    const std::uint32_t headerSize = withAlpha ? kV4HeaderSize : kInfoHeaderSize;
    const auto width = static_cast<std::size_t>(image.width());
    const auto height = static_cast<std::size_t>(image.height());
    const std::size_t stride = strideOf(image.width(), bpp);

    std::size_t entries = 0;
    if (indexed)
        entries = std::max<std::size_t>(
            {std::min<std::size_t>(image.palette().size(), 256), std::size_t{image.maxIndex()} + 1, 1});
    const std::size_t dataOffset = kFileHeaderSize + headerSize + entries * 4;
    const std::size_t dataSize = stride * height;
    if (dataOffset + dataSize > 0xFFFFFFFFu)
        return Error{"BMP image is too large"};

    ByteWriter out(dataOffset + dataSize);
    out.text("BM");
    out.u32(static_cast<std::uint32_t>(dataOffset + dataSize));
    out.u32(0);
    out.u32(static_cast<std::uint32_t>(dataOffset));
    out.u32(headerSize);
    out.s32(image.width());
    out.s32(image.height()); // positive: bottom-up
    out.u16(1);
    out.u16(static_cast<std::uint16_t>(bpp));
    out.u32(static_cast<std::uint32_t>(withAlpha ? BmpCompression::Bitfields : BmpCompression::Rgb));
    out.u32(static_cast<std::uint32_t>(dataSize));
    out.s32(pixelsPerMeter(options.dpi));
    out.s32(pixelsPerMeter(options.dpi));
    out.u32(static_cast<std::uint32_t>(entries));
    out.u32(0);
    if (withAlpha) {
        out.u32(0x00FF0000);
        out.u32(0x0000FF00);
        out.u32(0x000000FF);
        out.u32(0xFF000000);
        out.text("BGRs");   // LCS_sRGB, stored little-endian
        out.zeros(36 + 12); // endpoints and gamma, unused with sRGB
    }
    for (std::size_t i = 0; i < entries; ++i) {
        const Rgba c = i < image.palette().size() ? image.palette()[i] : rgb(0, 0, 0);
        out.u8(c.b);
        out.u8(c.g);
        out.u8(c.r);
        out.u8(0);
    }

    const Image source = indexed ? Image() : image.toRgba(options.transparentIndex0);
    std::vector<std::uint8_t> line(stride, 0);
    for (std::size_t row = 0; row < height; ++row) {
        const std::size_t y = height - 1 - row;
        if (indexed) {
            std::copy_n(image.pixels().begin() + static_cast<std::ptrdiff_t>(y * width), width, line.begin());
        } else {
            const std::uint8_t* src = source.pixels().data() + y * width * 4;
            for (std::size_t x = 0; x < width; ++x, src += 4) {
                std::uint8_t* dst = line.data() + x * static_cast<std::size_t>(bpp / 8);
                dst[0] = src[2];
                dst[1] = src[1];
                dst[2] = src[0];
                if (withAlpha)
                    dst[3] = src[3];
            }
        }
        out.bytes(std::span<const std::uint8_t>(line));
    }
    return out.take();
}

} // namespace OFS::Formats
