#include "formats/codecs/Png.h"

#include "formats/util/ByteReader.h"

#include <lodepng.h>

#include <algorithm>
#include <cstdlib>
#include <string>

namespace OFS::Formats {

namespace {

constexpr std::uint64_t kMaxPixels = std::uint64_t{1} << 26;

struct LodeState
{
    LodePNGState state;
    LodeState() { lodepng_state_init(&state); }
    ~LodeState() { lodepng_state_cleanup(&state); }
    LodeState(const LodeState&) = delete;
    LodeState& operator=(const LodeState&) = delete;
};

struct LodeBuffer
{
    unsigned char* data = nullptr;
    ~LodeBuffer() { std::free(data); }
};

Error lodeError(unsigned code)
{
    return Error{std::string("PNG: ") + lodepng_error_text(code)};
}

const unsigned char* bytesOf(ByteSpan data)
{
    return reinterpret_cast<const unsigned char*>(data.data());
}

int paletteEntries(ByteSpan data)
{
    ByteReader r(data);
    r.skip(8);
    while (r.ok() && r.remaining() >= 12) {
        const std::uint32_t length = r.u32be();
        const ByteSpan type = r.bytes(4);
        if (type.size() != 4 || length > r.remaining())
            break;
        if (toText(type) == "PLTE")
            return static_cast<int>(length / 3);
        if (toText(type) == "IDAT")
            break;
        r.skip(std::size_t{length} + 4);
    }
    return 0;
}

} // namespace

Result<PngInfo> readPngInfo(ByteSpan data)
{
    LodeState s;
    unsigned width = 0;
    unsigned height = 0;
    const unsigned error = lodepng_inspect(&width, &height, &s.state, bytesOf(data), data.size());
    if (error)
        return lodeError(error);
    if (width == 0 || height == 0 || std::uint64_t{width} * height > kMaxPixels)
        return Error{"PNG image size is out of range"};
    PngInfo info;
    info.width = static_cast<int>(width);
    info.height = static_cast<int>(height);
    info.bitDepth = static_cast<int>(s.state.info_png.color.bitdepth);
    info.colorType = static_cast<int>(s.state.info_png.color.colortype);
    info.paletteSize = paletteEntries(data);
    return info;
}

Result<Image> decodePng(ByteSpan data)
{
    auto info = readPngInfo(data);
    if (!info)
        return info.error();

    LodeState s;
    LodeBuffer out;
    unsigned width = 0;
    unsigned height = 0;
    if (info->colorType == static_cast<int>(LCT_PALETTE)) {
        // No color conversion, so the indices stay as stored.
        s.state.decoder.color_convert = 0;
        const unsigned error = lodepng_decode(&out.data, &width, &height, &s.state, bytesOf(data), data.size());
        if (error)
            return lodeError(error);
        const LodePNGColorMode& mode = s.state.info_png.color;
        const unsigned depth = mode.bitdepth;
        if (depth != 1 && depth != 2 && depth != 4 && depth != 8)
            return Error{"PNG: unsupported palette bit depth"};
        const std::size_t count = std::size_t{width} * height;
        std::vector<std::uint8_t> pixels(count);
        const unsigned mask = (1u << depth) - 1;
        for (std::size_t i = 0; i < count; ++i) {
            const std::size_t bit = i * depth;
            const unsigned shift = 8 - depth - static_cast<unsigned>(bit % 8);
            pixels[i] = static_cast<std::uint8_t>((out.data[bit / 8] >> shift) & mask);
        }
        Palette palette(mode.palettesize);
        for (std::size_t i = 0; i < mode.palettesize; ++i)
            palette[i] =
                rgb(mode.palette[i * 4], mode.palette[i * 4 + 1], mode.palette[i * 4 + 2], mode.palette[i * 4 + 3]);
        return Image::indexed(static_cast<int>(width), static_cast<int>(height), std::move(pixels), std::move(palette));
    }

    s.state.info_raw.colortype = LCT_RGBA;
    s.state.info_raw.bitdepth = 8;
    const unsigned error = lodepng_decode(&out.data, &width, &height, &s.state, bytesOf(data), data.size());
    if (error)
        return lodeError(error);
    const std::size_t size = std::size_t{width} * height * 4;
    return Image::rgba(static_cast<int>(width), static_cast<int>(height),
                       std::vector<std::uint8_t>(out.data, out.data + size));
}

Result<Bytes> encodePng(const Image& image, const PngEncodeOptions& options)
{
    if (image.isNull())
        return Error{"cannot encode an empty image as PNG"};

    LodeState s;
    s.state.encoder.auto_convert = 0;
    LodeBuffer out;
    std::size_t outSize = 0;
    unsigned error = 0;

    if (image.isIndexed() && options.color == PngColor::Automatic) {
        const Palette& palette = image.palette();
        const std::size_t entries =
            std::max<std::size_t>({std::min<std::size_t>(palette.size(), 256), std::size_t{image.maxIndex()} + 1, 1});
        for (LodePNGColorMode* mode : {&s.state.info_png.color, &s.state.info_raw}) {
            mode->colortype = LCT_PALETTE;
            mode->bitdepth = 8;
            for (std::size_t i = 0; i < entries; ++i) {
                const Rgba c = i < palette.size() ? palette[i] : rgb(0, 0, 0);
                const unsigned char alpha = (options.transparentIndex0 && i == 0) ? 0 : 255;
                error = lodepng_palette_add(mode, c.r, c.g, c.b, alpha);
                if (error)
                    return lodeError(error);
            }
        }
        error = lodepng_encode(&out.data, &outSize, image.pixels().data(), static_cast<unsigned>(image.width()),
                               static_cast<unsigned>(image.height()), &s.state);
    } else {
        const Image rgba = image.toRgba(options.transparentIndex0);
        s.state.info_raw.colortype = LCT_RGBA;
        s.state.info_raw.bitdepth = 8;
        s.state.info_png.color.colortype = options.color == PngColor::Rgb ? LCT_RGB : LCT_RGBA;
        s.state.info_png.color.bitdepth = 8;
        error = lodepng_encode(&out.data, &outSize, rgba.pixels().data(), static_cast<unsigned>(rgba.width()),
                               static_cast<unsigned>(rgba.height()), &s.state);
    }
    if (error)
        return lodeError(error);
    const auto* begin = reinterpret_cast<const std::byte*>(out.data);
    return Bytes(begin, begin + outSize);
}

} // namespace OFS::Formats
