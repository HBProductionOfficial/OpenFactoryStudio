#pragma once

#include "formats/util/Bytes.h"

#include <cstddef>
#include <cstdint>
#include <span>

namespace OFS::Formats {

// Little-endian unless the name ends in "be". A read past the end returns zero and sets a sticky
// flag, so a parser can read a whole structure and check ok() once at the end.
class ByteReader
{
public:
    ByteReader() = default;
    explicit ByteReader(ByteSpan buffer) noexcept
        : m_data(buffer)
    {}

    ByteSpan data() const noexcept { return m_data; }
    std::size_t size() const noexcept { return m_data.size(); }
    std::size_t position() const noexcept { return m_pos; }
    std::size_t remaining() const noexcept { return m_data.size() - m_pos; }
    bool atEnd() const noexcept { return m_pos >= m_data.size(); }

    bool ok() const noexcept { return !m_overflow; }
    bool overflowed() const noexcept { return m_overflow; }

    bool canRead(std::size_t count) const noexcept { return count <= remaining(); }

    bool seek(std::size_t offset) noexcept
    {
        if (offset > m_data.size()) {
            m_overflow = true;
            return false;
        }
        m_pos = offset;
        return true;
    }

    bool skip(std::size_t count) noexcept
    {
        if (!canRead(count)) {
            m_overflow = true;
            return false;
        }
        m_pos += count;
        return true;
    }

    std::uint8_t u8() noexcept
    {
        if (!canRead(1)) {
            m_overflow = true;
            return 0;
        }
        return byteAt(m_pos++);
    }

    std::int8_t s8() noexcept { return static_cast<std::int8_t>(u8()); }

    std::uint16_t u16() noexcept
    {
        if (!canRead(2)) {
            m_overflow = true;
            return 0;
        }
        const auto v = static_cast<std::uint16_t>(byteAt(m_pos) | (byteAt(m_pos + 1) << 8));
        m_pos += 2;
        return v;
    }

    std::int16_t s16() noexcept { return static_cast<std::int16_t>(u16()); }

    std::uint32_t u32() noexcept
    {
        if (!canRead(4)) {
            m_overflow = true;
            return 0;
        }
        const std::uint32_t v = std::uint32_t{byteAt(m_pos)} | (std::uint32_t{byteAt(m_pos + 1)} << 8) |
                                (std::uint32_t{byteAt(m_pos + 2)} << 16) | (std::uint32_t{byteAt(m_pos + 3)} << 24);
        m_pos += 4;
        return v;
    }

    std::int32_t s32() noexcept { return static_cast<std::int32_t>(u32()); }

    std::uint16_t u16be() noexcept
    {
        if (!canRead(2)) {
            m_overflow = true;
            return 0;
        }
        const auto v = static_cast<std::uint16_t>((byteAt(m_pos) << 8) | byteAt(m_pos + 1));
        m_pos += 2;
        return v;
    }

    std::uint32_t u32be() noexcept
    {
        if (!canRead(4)) {
            m_overflow = true;
            return 0;
        }
        const std::uint32_t v = (std::uint32_t{byteAt(m_pos)} << 24) | (std::uint32_t{byteAt(m_pos + 1)} << 16) |
                                (std::uint32_t{byteAt(m_pos + 2)} << 8) | std::uint32_t{byteAt(m_pos + 3)};
        m_pos += 4;
        return v;
    }

    ByteSpan bytes(std::size_t count) noexcept
    {
        if (!canRead(count)) {
            m_overflow = true;
            return {};
        }
        ByteSpan view = m_data.subspan(m_pos, count);
        m_pos += count;
        return view;
    }

    bool read(std::span<std::byte> out) noexcept
    {
        const ByteSpan view = bytes(out.size());
        if (view.size() != out.size())
            return false;
        for (std::size_t i = 0; i < out.size(); ++i)
            out[i] = view[i];
        return true;
    }

    ByteSpan rest() const noexcept { return m_data.subspan(m_pos); }

private:
    std::uint8_t byteAt(std::size_t index) const noexcept { return static_cast<std::uint8_t>(m_data[index]); }

    ByteSpan m_data;
    std::size_t m_pos = 0;
    bool m_overflow = false;
};

} // namespace OFS::Formats
