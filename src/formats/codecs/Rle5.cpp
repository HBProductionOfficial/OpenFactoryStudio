#include "formats/codecs/Rle5.h"

#include <algorithm>
#include <string>

namespace OFS::Formats {

namespace {

constexpr std::size_t kMaxFirstRun = 256; // run length byte + 1
constexpr std::size_t kMaxExtraRun = 8;   // three bits + 1
constexpr std::size_t kMaxExtras = 0x7F;  // seven bits

Error shortInput(std::size_t produced, std::size_t total)
{
    return Error{"RLE5 data ends after " + std::to_string(produced) + " of " + std::to_string(total) + " pixels"};
}

struct Run
{
    std::uint8_t color;
    std::size_t length;
};

} // namespace

Result<std::vector<std::uint8_t>> decodeRle5(ByteSpan data, std::size_t pixelCount, DecodeMode mode)
{
    const bool strict = mode == DecodeMode::Strict;
    // One input byte gives at most 128 pixels, so refuse hopeless input before allocating.
    if (strict && pixelCount / 128 > data.size())
        return shortInput(0, pixelCount);
    std::vector<std::uint8_t> out(pixelCount, 0);
    const std::span<const std::uint8_t> in = asU8(data);
    std::size_t pos = 0;
    std::size_t produced = 0;
    bool overflow = false;

    auto emit = [&](std::uint8_t color, std::size_t length) {
        if (length > pixelCount - produced) {
            if (strict) {
                overflow = true;
                return false;
            }
            length = pixelCount - produced;
        }
        std::fill_n(out.begin() + static_cast<std::ptrdiff_t>(produced), length, color);
        produced += length;
        return true;
    };

    while (produced < pixelCount) {
        if (in.size() - pos < 2) {
            if (strict)
                return shortInput(produced, pixelCount);
            break;
        }
        const std::size_t firstRun = std::size_t{in[pos]} + 1;
        const std::uint8_t lengthByte = in[pos + 1];
        pos += 2;
        const std::size_t extras = lengthByte & kMaxExtras;
        std::uint8_t color = 0;
        if (lengthByte & 0x80) {
            if (pos >= in.size()) {
                if (strict)
                    return shortInput(produced, pixelCount);
                break;
            }
            color = in[pos++];
        }
        if (!emit(color, firstRun))
            break;
        bool truncated = false;
        for (std::size_t k = 0; k < extras; ++k) {
            if (pos >= in.size()) {
                truncated = true;
                break;
            }
            const std::uint8_t b = in[pos++];
            if (!emit(static_cast<std::uint8_t>(b & 0x1F), std::size_t{static_cast<std::uint8_t>(b >> 5)} + 1))
                break;
        }
        if (overflow)
            break;
        if (truncated) {
            if (strict)
                return shortInput(produced, pixelCount);
            break;
        }
    }
    if (overflow)
        return Error{"RLE5 run goes past the end of the image"};
    return out;
}

Result<Bytes> encodeRle5(std::span<const std::uint8_t> pixels)
{
    std::vector<Run> runs;
    for (std::size_t i = 0; i < pixels.size();) {
        const std::uint8_t color = pixels[i];
        if (color > 31)
            return Error{"RLE5 can only store palette indices 0-31 (found " + std::to_string(color) + ")"};
        std::size_t length = 1;
        while (i + length < pixels.size() && length < kMaxFirstRun && pixels[i + length] == color)
            ++length;
        runs.push_back(Run{color, length});
        i += length;
    }

    Bytes out;
    out.reserve(pixels.size() / 2 + 16);
    std::vector<std::uint8_t> extras;
    std::size_t i = 0;
    while (i < runs.size()) {
        const Run first = runs[i++];
        extras.clear();
        // A long run starts a new packet, which costs less than many extra-run bytes.
        while (i < runs.size()) {
            const Run& next = runs[i];
            const std::size_t pieces = (next.length + kMaxExtraRun - 1) / kMaxExtraRun;
            if (pieces > 3 || extras.size() + pieces > kMaxExtras)
                break;
            std::size_t left = next.length;
            while (left > 0) {
                const std::size_t n = std::min(left, kMaxExtraRun);
                extras.push_back(static_cast<std::uint8_t>(((n - 1) << 5) | next.color));
                left -= n;
            }
            ++i;
        }
        const bool explicitColor = first.color != 0;
        out.push_back(std::byte{static_cast<std::uint8_t>(first.length - 1)});
        out.push_back(std::byte{static_cast<std::uint8_t>((explicitColor ? 0x80 : 0x00) | extras.size())});
        if (explicitColor)
            out.push_back(std::byte{first.color});
        for (const std::uint8_t b : extras)
            out.push_back(std::byte{b});
    }
    return out;
}

} // namespace OFS::Formats
