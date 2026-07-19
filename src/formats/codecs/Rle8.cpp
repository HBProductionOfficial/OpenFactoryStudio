#include "formats/codecs/Rle8.h"

#include <algorithm>
#include <string>

namespace OFS::Formats {

namespace {

constexpr std::size_t kMaxRun = 0x3F;

bool isRunMarker(std::uint8_t b)
{
    return (b & 0xC0) == 0x40;
}

Error shortInput(std::size_t produced, std::size_t total)
{
    return Error{"RLE8 data ends after " + std::to_string(produced) + " of " + std::to_string(total) + " pixels"};
}

} // namespace

Result<std::vector<std::uint8_t>> decodeRle8(ByteSpan data, std::size_t pixelCount, DecodeMode mode)
{
    const bool strict = mode == DecodeMode::Strict;
    // Two input bytes give at most 63 pixels, so refuse hopeless input before allocating.
    if (strict && pixelCount / 32 > data.size())
        return shortInput(0, pixelCount);
    std::vector<std::uint8_t> out(pixelCount, 0);
    const std::span<const std::uint8_t> in = asU8(data);
    std::size_t pos = 0;
    std::size_t produced = 0;
    while (produced < pixelCount) {
        if (pos >= in.size()) {
            if (strict)
                return shortInput(produced, pixelCount);
            break;
        }
        const std::uint8_t b = in[pos++];
        std::size_t count = 1;
        std::uint8_t value = b;
        if (isRunMarker(b)) {
            if (pos >= in.size()) {
                if (strict)
                    return Error{"RLE8 run at the end of the data has no pixel value"};
                break;
            }
            count = b & kMaxRun;
            value = in[pos++];
        }
        if (count > pixelCount - produced) {
            if (strict)
                return Error{"RLE8 run goes past the end of the image"};
            count = pixelCount - produced;
        }
        std::fill_n(out.begin() + static_cast<std::ptrdiff_t>(produced), count, value);
        produced += count;
    }
    return out;
}

Bytes encodeRle8(std::span<const std::uint8_t> pixels)
{
    Bytes out;
    out.reserve(pixels.size() / 2 + 16);
    std::size_t i = 0;
    while (i < pixels.size()) {
        const std::uint8_t value = pixels[i];
        std::size_t run = 1;
        while (i + run < pixels.size() && run < kMaxRun && pixels[i + run] == value)
            ++run;
        if (run == 1 && !isRunMarker(value)) {
            out.push_back(std::byte{value});
        } else {
            out.push_back(std::byte{static_cast<std::uint8_t>(0x40 | run)});
            out.push_back(std::byte{value});
        }
        i += run;
    }
    return out;
}

} // namespace OFS::Formats
