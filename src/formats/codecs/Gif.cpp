#include "formats/codecs/Gif.h"

#include "formats/codecs/Lzw.h"
#include "formats/util/ByteReader.h"
#include "formats/util/ByteWriter.h"

#include <algorithm>
#include <cstring>
#include <optional>
#include <string>

namespace OFS::Formats {

namespace {

constexpr std::uint8_t kExtension = 0x21;
constexpr std::uint8_t kImage = 0x2C;
constexpr std::uint8_t kTrailer = 0x3B;
constexpr std::uint8_t kGraphicControl = 0xF9;
constexpr std::uint8_t kPlainText = 0x01;
constexpr std::uint8_t kApplication = 0xFF;
// So damaged headers can't ask for huge buffers.
constexpr std::uint64_t kMaxFramePixels = std::uint64_t{1} << 26;
constexpr std::uint64_t kMaxTotalPixels = std::uint64_t{1} << 28;

struct GraphicControl
{
    int delay = 0;
    GifDisposal disposal = GifDisposal::Unspecified;
    int transparentIndex = -1;
};

// Returns false when the data ends before the closing empty block.
bool readSubBlocks(ByteReader& r, std::vector<ByteSpan>& blocks)
{
    blocks.clear();
    for (;;) {
        if (r.atEnd())
            return false;
        const std::uint8_t size = r.u8();
        if (size == 0)
            return true;
        if (!r.canRead(size)) {
            blocks.push_back(r.rest());
            r.skip(r.remaining());
            return false;
        }
        blocks.push_back(r.bytes(size));
    }
}

Palette readColorTable(ByteReader& r, int sizeBits)
{
    Palette table(std::size_t{1} << (sizeBits + 1));
    for (Rgba& c : table) {
        c.r = r.u8();
        c.g = r.u8();
        c.b = r.u8();
    }
    return table;
}

// Row order of interlaced images: every 8th row from 0, every 8th from 4, every 4th from 2,
// every 2nd from 1.
std::vector<int> interlacedRows(int height)
{
    std::vector<int> rows;
    rows.reserve(static_cast<std::size_t>(height));
    static constexpr int kStart[] = {0, 4, 2, 1};
    static constexpr int kStep[] = {8, 8, 4, 2};
    for (int pass = 0; pass < 4; ++pass) {
        for (int y = kStart[pass]; y < height; y += kStep[pass])
            rows.push_back(y);
    }
    return rows;
}

std::string hexByte(std::uint8_t value)
{
    static constexpr char kDigits[] = "0123456789ABCDEF";
    return std::string("0x") + kDigits[value >> 4] + kDigits[value & 0x0F];
}

Result<GifDocument> parseGif(ByteSpan data, DecodeMode mode, std::size_t maxFrames)
{
    const bool strict = mode == DecodeMode::Strict;
    ByteReader r(data);
    const std::string signature = toText(r.bytes(6));
    if (signature != "GIF87a" && signature != "GIF89a")
        return Error{"not a GIF image"};

    GifDocument gif;
    gif.width = r.u16();
    gif.height = r.u16();
    const std::uint8_t screenFlags = r.u8();
    gif.backgroundIndex = r.u8();
    r.u8(); // pixel aspect ratio
    if (screenFlags & 0x80)
        gif.globalPalette = readColorTable(r, screenFlags & 0x07);
    if (!r.ok())
        return Error{"GIF header is truncated"};

    std::optional<GraphicControl> control;
    std::vector<ByteSpan> blocks;
    std::uint64_t totalPixels = 0;
    bool ended = false;
    auto truncated = [&](const char* what) -> std::optional<Error> {
        if (strict || gif.frames.empty())
            return Error{std::string("GIF data is truncated (") + what + ")"};
        ended = true;
        return std::nullopt;
    };

    while (!ended && gif.frames.size() < maxFrames) {
        if (r.atEnd()) {
            if (auto e = truncated("no trailer"))
                return *e;
            break;
        }
        const std::uint8_t introducer = r.u8();
        if (introducer == kTrailer)
            break;

        if (introducer == kExtension) {
            const std::uint8_t label = r.u8();
            const bool complete = r.ok() && readSubBlocks(r, blocks);
            if (!complete) {
                if (auto e = truncated("extension"))
                    return *e;
                break;
            }
            if (label == kGraphicControl) {
                if (blocks.empty() || blocks[0].size() < 4) {
                    if (strict)
                        return Error{"GIF graphic control extension is too short"};
                    continue;
                }
                const std::span<const std::uint8_t> gce = asU8(blocks[0]);
                const auto disposal = static_cast<std::uint8_t>((gce[0] >> 2) & 0x07);
                if (disposal > 3 && strict)
                    return Error{"unknown GIF disposal method " + std::to_string(disposal)};
                GraphicControl gc;
                gc.disposal = disposal > 3 ? GifDisposal::Unspecified : static_cast<GifDisposal>(disposal);
                gc.delay = gce[1] | (gce[2] << 8);
                gc.transparentIndex = (gce[0] & 0x01) ? int{gce[3]} : -1;
                control = gc;
            } else if (label == kApplication) {
                if (blocks.size() >= 2 && blocks[0].size() == 11) {
                    const std::string id = toText(blocks[0]);
                    const std::span<const std::uint8_t> sub = asU8(blocks[1]);
                    if ((id == "NETSCAPE2.0" || id == "ANIMEXTS1.0") && sub.size() >= 3 && sub[0] == 1)
                        gif.loopCount = sub[1] | (sub[2] << 8);
                }
            } else if (label == kPlainText) {
                control.reset(); // a graphic control extension applies to the next rendering block
            }
            continue;
        }

        if (introducer != kImage) {
            if (strict)
                return Error{"unknown GIF block " + hexByte(introducer)};
            break;
        }

        GifFrame frame;
        frame.left = r.u16();
        frame.top = r.u16();
        const int width = r.u16();
        const int height = r.u16();
        const std::uint8_t flags = r.u8();
        frame.localPalette = (flags & 0x80) != 0;
        frame.interlaced = (flags & 0x40) != 0;
        Palette palette;
        if (frame.localPalette)
            palette = readColorTable(r, flags & 0x07);
        const int minCodeSize = r.u8();
        if (!r.ok()) {
            if (auto e = truncated("image descriptor"))
                return *e;
            break;
        }
        const bool complete = readSubBlocks(r, blocks);
        if (!complete && strict)
            return Error{"GIF data is truncated (image data)"};

        const std::uint64_t pixels = std::uint64_t(width) * std::uint64_t(height);
        totalPixels += pixels;
        if (pixels > kMaxFramePixels || totalPixels > kMaxTotalPixels)
            return Error{"GIF image is too large to decode"};
        if (palette.empty())
            palette = gif.globalPalette.empty() ? grayscalePalette() : gif.globalPalette;
        if (control) {
            frame.delay = control->delay;
            frame.disposal = control->disposal;
            frame.transparentIndex = control->transparentIndex;
            control.reset();
        }
        if (frame.transparentIndex >= 0 && static_cast<std::size_t>(frame.transparentIndex) < palette.size())
            palette[static_cast<std::size_t>(frame.transparentIndex)].a = 0;

        Bytes code;
        for (const ByteSpan block : blocks)
            code.insert(code.end(), block.begin(), block.end());
        const auto fill = static_cast<std::uint8_t>(std::max(frame.transparentIndex, 0));
        std::vector<std::uint8_t> indices(static_cast<std::size_t>(pixels), fill);
        auto decoded = decodeGifLzw(code, minCodeSize, indices, complete ? mode : DecodeMode::Lenient);
        if (!decoded)
            return withContext("GIF frame " + std::to_string(gif.frames.size()), decoded.error());
        if (frame.interlaced && height > 1) {
            std::vector<std::uint8_t> ordered(indices.size());
            const std::vector<int> rows = interlacedRows(height);
            const auto rowBytes = static_cast<std::size_t>(width);
            for (std::size_t i = 0; i < rows.size(); ++i)
                std::memcpy(ordered.data() + static_cast<std::size_t>(rows[i]) * rowBytes,
                            indices.data() + i * rowBytes, rowBytes);
            indices = std::move(ordered);
        }
        frame.image = Image::indexed(width, height, std::move(indices), std::move(palette));
        gif.frames.push_back(std::move(frame));
        if (!complete)
            break;
    }

    if (gif.frames.empty())
        return Error{"GIF image has no frames"};
    if (gif.width == 0 || gif.height == 0) {
        for (const GifFrame& f : gif.frames) {
            gif.width = std::max(gif.width, f.left + f.image.width());
            gif.height = std::max(gif.height, f.top + f.image.height());
        }
    }
    if (std::uint64_t(gif.width) * std::uint64_t(gif.height) > kMaxFramePixels)
        return Error{"GIF screen is too large"};
    return gif;
}

int tableBits(std::size_t entries)
{
    int bits = 1;
    while ((std::size_t{1} << bits) < entries)
        ++bits;
    return bits;
}

void writeColorTable(ByteWriter& out, const Palette& palette, int bits)
{
    const std::size_t count = std::size_t{1} << bits;
    for (std::size_t i = 0; i < count; ++i) {
        const Rgba c = i < palette.size() ? palette[i] : rgb(0, 0, 0);
        out.u8(c.r);
        out.u8(c.g);
        out.u8(c.b);
    }
}

bool sharesGlobalColors(const Palette& own, const Palette& global, std::size_t used)
{
    if (used > global.size())
        return false;
    for (std::size_t i = 0; i < std::min(own.size(), global.size()); ++i) {
        if (!sameRgb(own[i], global[i]))
            return false;
    }
    return own.size() <= global.size();
}

} // namespace

Result<GifDocument> readGif(ByteSpan data, DecodeMode mode, std::size_t maxFrames)
{
    return parseGif(data, mode, std::max<std::size_t>(maxFrames, 1));
}

Result<Bytes> writeGif(const GifDocument& gif)
{
    if (gif.frames.empty())
        return Error{"a GIF needs at least one frame"};
    if (gif.globalPalette.size() > 256)
        return Error{"a GIF color table holds at most 256 colors"};

    int width = std::max(gif.width, 0);
    int height = std::max(gif.height, 0);
    for (std::size_t i = 0; i < gif.frames.size(); ++i) {
        const GifFrame& f = gif.frames[i];
        const std::string where = "GIF frame " + std::to_string(i) + ": ";
        if (!f.image.isIndexed())
            return Error{where + "needs an indexed image"};
        if (f.image.isNull() || f.image.width() > 0xFFFF || f.image.height() > 0xFFFF)
            return Error{where + "must be between 1x1 and 65535x65535 pixels"};
        if (f.left < 0 || f.top < 0 || f.left > 0xFFFF || f.top > 0xFFFF)
            return Error{where + "position is out of range"};
        if (f.image.palette().size() > 256)
            return Error{where + "a GIF color table holds at most 256 colors"};
        if (f.transparentIndex > 255)
            return Error{where + "transparent index is out of range"};
        width = std::max(width, f.left + f.image.width());
        height = std::max(height, f.top + f.image.height());
    }
    if (width <= 0 || height <= 0 || width > 0xFFFF || height > 0xFFFF)
        return Error{"GIF screen size is out of range"};

    ByteWriter out;
    out.text("GIF89a");
    out.u16(static_cast<std::uint16_t>(width));
    out.u16(static_cast<std::uint16_t>(height));
    const bool hasGlobal = !gif.globalPalette.empty();
    const int globalBits = hasGlobal ? tableBits(std::max<std::size_t>(gif.globalPalette.size(), 2)) : 0;
    out.u8(static_cast<std::uint8_t>(hasGlobal ? 0x80 | 0x70 | (globalBits - 1) : 0x70));
    out.u8(gif.backgroundIndex);
    out.u8(0); // pixel aspect ratio
    if (hasGlobal)
        writeColorTable(out, gif.globalPalette, globalBits);

    if (gif.loopCount >= 0) {
        out.u8(kExtension);
        out.u8(kApplication);
        out.u8(11);
        out.text("NETSCAPE2.0");
        out.u8(3);
        out.u8(1);
        out.u16(static_cast<std::uint16_t>(std::min(gif.loopCount, 0xFFFF)));
        out.u8(0);
    }

    for (const GifFrame& f : gif.frames) {
        const Palette& palette = f.image.palette();
        const std::size_t used = std::size_t{f.image.maxIndex()} + 1;
        const bool local = f.localPalette || !hasGlobal || !sharesGlobalColors(palette, gif.globalPalette, used) ||
                           used > (std::size_t{1} << globalBits);
        const int bits = local ? tableBits(std::max<std::size_t>({palette.size(), used, 2})) : globalBits;

        const bool animation = gif.frames.size() > 1;
        if (animation || f.delay != 0 || f.disposal != GifDisposal::Unspecified || f.transparentIndex >= 0) {
            out.u8(kExtension);
            out.u8(kGraphicControl);
            out.u8(4);
            const auto disposal = static_cast<std::uint8_t>(f.disposal);
            out.u8(static_cast<std::uint8_t>(((disposal & 0x07) << 2) | (f.transparentIndex >= 0 ? 1 : 0)));
            out.u16(static_cast<std::uint16_t>(std::clamp(f.delay, 0, 0xFFFF)));
            out.u8(static_cast<std::uint8_t>(std::max(f.transparentIndex, 0)));
            out.u8(0);
        }

        out.u8(kImage);
        out.u16(static_cast<std::uint16_t>(f.left));
        out.u16(static_cast<std::uint16_t>(f.top));
        out.u16(static_cast<std::uint16_t>(f.image.width()));
        out.u16(static_cast<std::uint16_t>(f.image.height()));
        auto flags = static_cast<std::uint8_t>(f.interlaced ? 0x40 : 0x00);
        if (local)
            flags = static_cast<std::uint8_t>(flags | 0x80 | (bits - 1));
        out.u8(flags);
        if (local)
            writeColorTable(out, palette, bits);

        const int minCodeSize = std::max(2, bits);
        std::span<const std::uint8_t> indices = f.image.pixels();
        std::vector<std::uint8_t> reordered;
        if (f.interlaced && f.image.height() > 1) {
            const std::vector<int> rows = interlacedRows(f.image.height());
            const auto rowBytes = static_cast<std::size_t>(f.image.width());
            reordered.resize(indices.size());
            for (std::size_t i = 0; i < rows.size(); ++i)
                std::memcpy(reordered.data() + i * rowBytes,
                            indices.data() + static_cast<std::size_t>(rows[i]) * rowBytes, rowBytes);
            indices = reordered;
        }
        auto code = encodeGifLzw(indices, minCodeSize);
        if (!code)
            return code.error();
        out.u8(static_cast<std::uint8_t>(minCodeSize));
        const ByteSpan stream = *code;
        for (std::size_t at = 0; at < stream.size(); at += 255) {
            const std::size_t n = std::min<std::size_t>(255, stream.size() - at);
            out.u8(static_cast<std::uint8_t>(n));
            out.bytes(stream.subspan(at, n));
        }
        out.u8(0);
    }
    out.u8(kTrailer);
    return out.take();
}

GifFrame gifFrameOnScreen(const GifDocument& gif, std::size_t index)
{
    GifFrame placed;
    if (index >= gif.frames.size())
        return placed;
    const GifFrame& f = gif.frames[index];
    placed.delay = f.delay;
    placed.disposal = f.disposal;
    placed.transparentIndex = f.transparentIndex;
    placed.localPalette = f.localPalette;
    const std::uint8_t fill =
        f.transparentIndex >= 0 ? static_cast<std::uint8_t>(f.transparentIndex) : gif.backgroundIndex;
    Image screen(gif.width, gif.height, PixelFormat::Indexed8);
    std::fill(screen.pixelBuffer().begin(), screen.pixelBuffer().end(), fill);
    screen.setPalette(f.image.palette());
    const int x0 = std::max(0, f.left);
    const int x1 = std::min(gif.width, f.left + f.image.width());
    for (int y = std::max(0, f.top); y < std::min(gif.height, f.top + f.image.height()); ++y) {
        for (int x = x0; x < x1; ++x)
            screen.setIndex(x, y, f.image.indexAt(x - f.left, y - f.top));
    }
    placed.image = std::move(screen);
    return placed;
}

std::vector<Image> renderGifFrames(const GifDocument& gif)
{
    std::vector<Image> out;
    if (gif.width <= 0 || gif.height <= 0 ||
        std::uint64_t(gif.width) * std::uint64_t(gif.height) * gif.frames.size() > kMaxTotalPixels)
        return out;
    out.reserve(gif.frames.size());
    Image canvas(gif.width, gif.height, PixelFormat::Rgba32);
    for (const GifFrame& f : gif.frames) {
        std::optional<Image> saved;
        if (f.disposal == GifDisposal::Previous)
            saved = canvas;
        const Palette& palette = f.image.palette();
        const int x0 = std::max(0, f.left);
        const int x1 = std::min(gif.width, f.left + f.image.width());
        const int y0 = std::max(0, f.top);
        const int y1 = std::min(gif.height, f.top + f.image.height());
        for (int y = y0; y < y1; ++y) {
            for (int x = x0; x < x1; ++x) {
                const std::uint8_t index = f.image.indexAt(x - f.left, y - f.top);
                if (int{index} == f.transparentIndex)
                    continue;
                Rgba c = index < palette.size() ? palette[index] : rgb(0, 0, 0);
                c.a = 255;
                canvas.setColor(x, y, c);
            }
        }
        out.push_back(canvas);
        if (f.disposal == GifDisposal::Background) {
            for (int y = y0; y < y1; ++y) {
                for (int x = x0; x < x1; ++x)
                    canvas.setColor(x, y, rgb(0, 0, 0, 0));
            }
        } else if (saved) {
            canvas = std::move(*saved);
        }
    }
    return out;
}

Result<Image> decodeGif(ByteSpan data, DecodeMode mode)
{
    auto gif = parseGif(data, mode, 1);
    if (!gif)
        return gif.error();
    return gifFrameOnScreen(*gif, 0).image;
}

} // namespace OFS::Formats
