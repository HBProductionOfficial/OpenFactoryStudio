#include "formats/codecs/Lzw.h"

#include <algorithm>
#include <array>
#include <string>
#include <vector>

namespace OFS::Formats {

namespace {

constexpr unsigned kMaxCodes = 4096;
constexpr int kMaxWidth = 12;

bool validCodeSize(int minCodeSize)
{
    return minCodeSize >= 2 && minCodeSize <= 8;
}

class BitWriter
{
public:
    explicit BitWriter(Bytes& out)
        : m_out(out)
    {}

    void put(unsigned code, int width)
    {
        m_acc |= static_cast<std::uint32_t>(code) << m_bits;
        m_bits += width;
        while (m_bits >= 8) {
            m_out.push_back(std::byte{static_cast<std::uint8_t>(m_acc & 0xFF)});
            m_acc >>= 8;
            m_bits -= 8;
        }
    }

    void flush()
    {
        if (m_bits > 0)
            m_out.push_back(std::byte{static_cast<std::uint8_t>(m_acc & 0xFF)});
        m_acc = 0;
        m_bits = 0;
    }

private:
    Bytes& m_out;
    std::uint32_t m_acc = 0;
    int m_bits = 0;
};

// Open-addressing map from (prefix code, next index) to the code of that string.
class StringTable
{
public:
    StringTable()
        : m_keys(kSize, 0)
        , m_values(kSize, 0)
    {}

    void clear() { std::fill(m_keys.begin(), m_keys.end(), 0u); }

    int find(unsigned prefix, unsigned index) const
    {
        const std::uint32_t key = ((prefix << 8) | index) + 1;
        for (std::size_t slot = hash(key);; slot = (slot + 1) & (kSize - 1)) {
            if (m_keys[slot] == 0)
                return -1;
            if (m_keys[slot] == key)
                return m_values[slot];
        }
    }

    void insert(unsigned prefix, unsigned index, unsigned code)
    {
        const std::uint32_t key = ((prefix << 8) | index) + 1;
        std::size_t slot = hash(key);
        while (m_keys[slot] != 0)
            slot = (slot + 1) & (kSize - 1);
        m_keys[slot] = key;
        m_values[slot] = static_cast<std::uint16_t>(code);
    }

private:
    // Twice the number of codes, so probe sequences stay short.
    static constexpr std::size_t kSize = 8192;

    static std::size_t hash(std::uint32_t key) { return ((key * 2654435761u) >> 19) & (kSize - 1); }

    std::vector<std::uint32_t> m_keys; // key + 1, 0 = empty
    std::vector<std::uint16_t> m_values;
};

} // namespace

Result<Bytes> encodeGifLzw(std::span<const std::uint8_t> indices, int minCodeSize)
{
    if (!validCodeSize(minCodeSize))
        return Error{"LZW minimum code size must be between 2 and 8"};
    const unsigned clearCode = 1u << minCodeSize;
    const unsigned endCode = clearCode + 1;
    for (const std::uint8_t index : indices) {
        if (index >= clearCode)
            return Error{"pixel index " + std::to_string(index) + " does not fit an LZW code size of " +
                         std::to_string(minCodeSize)};
    }

    Bytes out;
    out.reserve(indices.size() / 2 + 16);
    BitWriter writer(out);
    StringTable table;
    int width = minCodeSize + 1;
    unsigned next = clearCode + 2;
    writer.put(clearCode, width);

    if (!indices.empty()) {
        unsigned prefix = indices[0];
        for (std::size_t i = 1; i < indices.size(); ++i) {
            const unsigned index = indices[i];
            const int code = table.find(prefix, index);
            if (code >= 0) {
                prefix = static_cast<unsigned>(code);
                continue;
            }
            writer.put(prefix, width);
            if (next < kMaxCodes) {
                table.insert(prefix, index, next);
                ++next;
                // The decoder adds this entry one code later, so it widens one step later too.
                if (next > (1u << width) && width < kMaxWidth)
                    ++width;
            } else {
                writer.put(clearCode, width);
                table.clear();
                width = minCodeSize + 1;
                next = clearCode + 2;
            }
            prefix = index;
        }
        writer.put(prefix, width);
        // Reading the last code makes the decoder add one more entry, which may widen the codes.
        if (next == (1u << width) && width < kMaxWidth)
            ++width;
    }
    writer.put(endCode, width);
    writer.flush();
    return out;
}

Result<std::size_t> decodeGifLzw(ByteSpan data, int minCodeSize, std::span<std::uint8_t> out, DecodeMode mode)
{
    if (!validCodeSize(minCodeSize))
        return Error{"LZW minimum code size must be between 2 and 8"};
    const bool strict = mode == DecodeMode::Strict;
    const unsigned clearCode = 1u << minCodeSize;
    const unsigned endCode = clearCode + 1;

    std::array<std::uint16_t, kMaxCodes> prefixes{};
    std::array<std::uint8_t, kMaxCodes> suffixes{};
    std::array<std::uint8_t, kMaxCodes> firsts{};
    for (unsigned i = 0; i < clearCode; ++i) {
        suffixes[i] = static_cast<std::uint8_t>(i);
        firsts[i] = static_cast<std::uint8_t>(i);
    }
    std::array<std::uint8_t, kMaxCodes> stack{};

    int width = minCodeSize + 1;
    unsigned next = clearCode + 2;
    int previous = -1;
    std::size_t produced = 0;
    std::size_t pos = 0;
    std::uint32_t acc = 0;
    int bits = 0;

    for (;;) {
        while (bits < width) {
            if (pos >= data.size()) {
                if (strict)
                    return Error{"LZW data ends before the end-of-information code"};
                return produced;
            }
            acc |= static_cast<std::uint32_t>(static_cast<std::uint8_t>(data[pos++])) << bits;
            bits += 8;
        }
        const unsigned code = acc & ((1u << width) - 1);
        acc >>= width;
        bits -= width;

        if (code == clearCode) {
            width = minCodeSize + 1;
            next = clearCode + 2;
            previous = -1;
            continue;
        }
        if (code == endCode) {
            if (strict && produced != out.size())
                return Error{"LZW data ends after " + std::to_string(produced) + " of " + std::to_string(out.size()) +
                             " pixels"};
            return produced;
        }

        if (previous < 0) {
            if (code >= clearCode) {
                if (strict)
                    return Error{"invalid LZW code " + std::to_string(code)};
                return produced;
            }
        } else {
            const auto prev = static_cast<unsigned>(previous);
            std::uint8_t first = 0;
            if (code < next) {
                first = firsts[code];
            } else if (code == next && next < kMaxCodes) {
                first = firsts[prev]; // the string is the previous one plus its own first index
            } else {
                if (strict)
                    return Error{"invalid LZW code " + std::to_string(code)};
                return produced;
            }
            if (next < kMaxCodes) {
                prefixes[next] = static_cast<std::uint16_t>(prev);
                suffixes[next] = first;
                firsts[next] = firsts[prev];
                ++next;
                if (next == (1u << width) && width < kMaxWidth)
                    ++width;
            }
        }

        std::size_t length = 0;
        for (unsigned c = code;; c = prefixes[c]) {
            stack[length++] = suffixes[c];
            if (c < clearCode)
                break;
        }
        const std::size_t room = out.size() - produced;
        if (length > room) {
            if (strict)
                return Error{"LZW data holds more than " + std::to_string(out.size()) + " pixels"};
            for (std::size_t k = 0; k < room; ++k)
                out[produced + k] = stack[length - 1 - k];
            return out.size();
        }
        for (std::size_t k = 0; k < length; ++k)
            out[produced + k] = stack[length - 1 - k];
        produced += length;
        previous = static_cast<int>(code);
    }
}

} // namespace OFS::Formats
