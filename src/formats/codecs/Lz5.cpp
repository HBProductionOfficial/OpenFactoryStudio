#include "formats/codecs/Lz5.h"

#include <algorithm>
#include <array>
#include <string>

namespace OFS::Formats {

namespace {

constexpr std::size_t kShortRunMax = 7;
constexpr std::size_t kLongRunMin = 8;
constexpr std::size_t kLongRunMax = 255 + kLongRunMin;
constexpr std::size_t kShortCopyMin = 2;
constexpr std::size_t kShortCopyMax = 64;
constexpr std::size_t kShortCopyMaxDistance = 256;
constexpr std::size_t kLongCopyMin = 3;
constexpr std::size_t kLongCopyMax = 255 + kLongCopyMin;
constexpr std::size_t kLongCopyMaxDistance = 1024;
constexpr int kChainDepth = 48;

Error shortInput(std::size_t produced, std::size_t total)
{
    return Error{"LZ5 data ends after " + std::to_string(produced) + " of " + std::to_string(total) + " pixels"};
}

struct Token
{
    bool copy = false;
    bool longForm = false;
    std::uint8_t color = 0;
    std::uint8_t carriedBits = 0;  // stored in bits 6-7 of a short copy
    bool implicitDistance = false; // fourth short copy of a group, no distance byte
    std::size_t length = 0;
    std::size_t distance = 0;
};

} // namespace

Result<std::vector<std::uint8_t>> decodeLz5(ByteSpan data, std::size_t pixelCount, DecodeMode mode)
{
    const bool strict = mode == DecodeMode::Strict;
    // One input byte gives at most 132 pixels, so refuse hopeless input before allocating.
    if (strict && pixelCount / 132 > data.size())
        return shortInput(0, pixelCount);
    std::vector<std::uint8_t> out(pixelCount, 0);
    const std::span<const std::uint8_t> in = asU8(data);
    std::size_t pos = 0;
    std::size_t produced = 0;
    std::uint8_t control = 0;
    int controlBitsLeft = 0;
    std::uint8_t carried = 0;
    int carriedPieces = 0;

    while (produced < pixelCount) {
        if (controlBitsLeft == 0) {
            if (pos >= in.size()) {
                if (strict)
                    return shortInput(produced, pixelCount);
                break;
            }
            control = in[pos++];
            controlBitsLeft = 8;
        }
        const bool isCopy = (control & 1) != 0;
        control = static_cast<std::uint8_t>(control >> 1);
        --controlBitsLeft;

        if (pos >= in.size()) {
            if (strict)
                return shortInput(produced, pixelCount);
            break;
        }
        const std::uint8_t first = in[pos++];
        std::size_t length = 0;

        if (isCopy) {
            std::size_t distance = 0;
            if ((first & 0x3F) == 0) {
                if (in.size() - pos < 2) {
                    if (strict)
                        return shortInput(produced, pixelCount);
                    break;
                }
                distance = ((std::size_t{static_cast<std::uint8_t>(first >> 6)} << 8) | in[pos]) + 1;
                length = std::size_t{in[pos + 1]} + kLongCopyMin;
                pos += 2;
            } else {
                carried = static_cast<std::uint8_t>(carried | ((first & 0xC0) >> (carriedPieces * 2)));
                ++carriedPieces;
                length = std::size_t{static_cast<std::uint8_t>(first & 0x3F)} + 1;
                if (carriedPieces < 4) {
                    if (pos >= in.size()) {
                        if (strict)
                            return shortInput(produced, pixelCount);
                        break;
                    }
                    distance = std::size_t{in[pos++]} + 1;
                } else {
                    distance = std::size_t{carried} + 1;
                    carried = 0;
                    carriedPieces = 0;
                }
            }
            if (distance > produced && strict)
                return Error{"LZ5 copy refers to data before the start of the image"};
            if (length > pixelCount - produced) {
                if (strict)
                    return Error{"LZ5 copy goes past the end of the image"};
                length = pixelCount - produced;
            }
            for (std::size_t k = 0; k < length; ++k, ++produced)
                out[produced] = distance <= produced ? out[produced - distance] : 0;
        } else {
            std::uint8_t color = 0;
            if ((first & 0xE0) == 0) {
                if (pos >= in.size()) {
                    if (strict)
                        return shortInput(produced, pixelCount);
                    break;
                }
                color = first;
                length = std::size_t{in[pos++]} + kLongRunMin;
            } else {
                color = static_cast<std::uint8_t>(first & 0x1F);
                length = std::size_t{static_cast<std::uint8_t>(first >> 5)};
            }
            if (length > pixelCount - produced) {
                if (strict)
                    return Error{"LZ5 run goes past the end of the image"};
                length = pixelCount - produced;
            }
            std::fill_n(out.begin() + static_cast<std::ptrdiff_t>(produced), length, color);
            produced += length;
        }
    }
    return out;
}

Result<Bytes> encodeLz5(std::span<const std::uint8_t> pixels)
{
    const std::size_t n = pixels.size();
    for (std::size_t i = 0; i < n; ++i) {
        if (pixels[i] > 31)
            return Error{"LZ5 can only store palette indices 0-31 (found " + std::to_string(pixels[i]) + ")"};
    }

    // Hash chains keyed by two pixels. They are 5 bits each, so the key is exact.
    constexpr std::ptrdiff_t kNone = -1;
    std::array<std::ptrdiff_t, 1024> head;
    head.fill(kNone);
    std::vector<std::ptrdiff_t> previous(n, kNone);
    auto keyAt = [&](std::size_t i) {
        return (std::size_t{pixels[i]} << 5) | pixels[i + 1];
    };
    std::size_t inserted = 0;
    auto insertUpTo = [&](std::size_t end) {
        for (; inserted < end; ++inserted) {
            if (inserted + 1 >= n)
                continue;
            const std::size_t key = keyAt(inserted);
            previous[inserted] = head[key];
            head[key] = static_cast<std::ptrdiff_t>(inserted);
        }
    };

    std::vector<Token> tokens;
    tokens.reserve(n / 4 + 8);
    std::size_t i = 0;
    while (i < n) {
        insertUpTo(i);

        std::size_t run = 1;
        while (i + run < n && run < kLongRunMax && pixels[i + run] == pixels[i])
            ++run;
        const std::size_t runCost = run <= kShortRunMax ? 1 : 2;

        std::size_t bestLength = 0;
        std::size_t bestDistance = 0;
        if (i + 1 < n) {
            const std::size_t limit = std::min(kLongCopyMax, n - i);
            std::ptrdiff_t candidate = head[keyAt(i)];
            for (int depth = 0; candidate != kNone && depth < kChainDepth; ++depth) {
                const std::size_t from = static_cast<std::size_t>(candidate);
                const std::size_t distance = i - from;
                if (distance > kLongCopyMaxDistance)
                    break;
                std::size_t length = 0;
                while (length < limit && pixels[from + length] == pixels[i + length])
                    ++length;
                std::size_t usable = 0;
                if (length >= kLongCopyMin)
                    usable = length;
                if (distance <= kShortCopyMaxDistance && length >= kShortCopyMin)
                    usable = length;
                if (usable > bestLength) {
                    bestLength = usable;
                    bestDistance = distance;
                    if (usable == limit)
                        break;
                }
                candidate = previous[from];
            }
        }

        bool useCopy = false;
        bool longForm = false;
        std::size_t copyLength = 0;
        if (bestLength >= kShortCopyMin) {
            const bool shortOk = bestDistance <= kShortCopyMaxDistance;
            const bool longOk = bestLength >= kLongCopyMin;
            if (shortOk && (bestLength <= kShortCopyMax || !longOk)) {
                copyLength = std::min(bestLength, kShortCopyMax);
            } else {
                longForm = true;
                copyLength = std::min(bestLength, kLongCopyMax);
            }
            const std::size_t copyCost = longForm ? 3 : 2;
            // Pick whichever covers more pixels per byte. Ties go to the longer one.
            const std::size_t lhs = copyLength * runCost;
            const std::size_t rhs = run * copyCost;
            useCopy = lhs > rhs || (lhs == rhs && copyLength > run);
        }

        Token token;
        if (useCopy) {
            token.copy = true;
            token.longForm = longForm;
            token.length = copyLength;
            token.distance = bestDistance;
        } else {
            token.color = pixels[i];
            token.length = run;
            token.longForm = run > kShortRunMax;
        }
        tokens.push_back(token);
        i += token.length;
    }

    // The fourth short copy's distance - 1 goes into the top two bits of its group of four.
    std::vector<std::size_t> shortCopies;
    for (std::size_t t = 0; t < tokens.size(); ++t) {
        if (tokens[t].copy && !tokens[t].longForm)
            shortCopies.push_back(t);
    }
    for (std::size_t g = 0; g + 4 <= shortCopies.size(); g += 4) {
        const auto spare = static_cast<std::uint8_t>(tokens[shortCopies[g + 3]].distance - 1);
        tokens[shortCopies[g]].carriedBits = static_cast<std::uint8_t>((spare >> 6) & 3);
        tokens[shortCopies[g + 1]].carriedBits = static_cast<std::uint8_t>((spare >> 4) & 3);
        tokens[shortCopies[g + 2]].carriedBits = static_cast<std::uint8_t>((spare >> 2) & 3);
        tokens[shortCopies[g + 3]].carriedBits = static_cast<std::uint8_t>(spare & 3);
        tokens[shortCopies[g + 3]].implicitDistance = true;
    }

    Bytes out;
    out.reserve(tokens.size() * 2 + tokens.size() / 8 + 8);
    auto put = [&out](std::size_t v) {
        out.push_back(std::byte{static_cast<std::uint8_t>(v)});
    };
    for (std::size_t t = 0; t < tokens.size(); ++t) {
        if (t % 8 == 0) {
            std::uint8_t control = 0;
            for (std::size_t k = 0; k < 8 && t + k < tokens.size(); ++k) {
                if (tokens[t + k].copy)
                    control = static_cast<std::uint8_t>(control | (1u << k));
            }
            put(control);
        }
        const Token& token = tokens[t];
        if (!token.copy) {
            if (!token.longForm) {
                put((token.length << 5) | token.color);
            } else {
                put(token.color);
                put(token.length - kLongRunMin);
            }
        } else if (token.longForm) {
            const std::size_t d = token.distance - 1;
            put((d >> 8) << 6);
            put(d & 0xFF);
            put(token.length - kLongCopyMin);
        } else {
            put((std::size_t{token.carriedBits} << 6) | (token.length - 1));
            if (!token.implicitDistance)
                put(token.distance - 1);
        }
    }
    return out;
}

} // namespace OFS::Formats
