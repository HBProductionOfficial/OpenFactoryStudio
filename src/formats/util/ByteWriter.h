#pragma once

#include "formats/util/Bytes.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <utility>

namespace OFS::Formats {

// Little-endian unless the name ends in "be".
class ByteWriter
{
public:
    ByteWriter() = default;
    explicit ByteWriter(std::size_t capacity) { m_buffer.reserve(capacity); }

    std::size_t size() const noexcept { return m_buffer.size(); }
    const Bytes& data() const noexcept { return m_buffer; }
    Bytes take() noexcept { return std::exchange(m_buffer, {}); }
    void reserve(std::size_t capacity) { m_buffer.reserve(capacity); }

    void u8(std::uint8_t v) { m_buffer.push_back(std::byte{v}); }
    void s8(std::int8_t v) { u8(static_cast<std::uint8_t>(v)); }

    void u16(std::uint16_t v)
    {
        u8(static_cast<std::uint8_t>(v & 0xFF));
        u8(static_cast<std::uint8_t>(v >> 8));
    }
    void s16(std::int16_t v) { u16(static_cast<std::uint16_t>(v)); }

    void u32(std::uint32_t v)
    {
        u8(static_cast<std::uint8_t>(v & 0xFF));
        u8(static_cast<std::uint8_t>((v >> 8) & 0xFF));
        u8(static_cast<std::uint8_t>((v >> 16) & 0xFF));
        u8(static_cast<std::uint8_t>(v >> 24));
    }
    void s32(std::int32_t v) { u32(static_cast<std::uint32_t>(v)); }

    void u16be(std::uint16_t v)
    {
        u8(static_cast<std::uint8_t>(v >> 8));
        u8(static_cast<std::uint8_t>(v & 0xFF));
    }

    void u32be(std::uint32_t v)
    {
        u8(static_cast<std::uint8_t>(v >> 24));
        u8(static_cast<std::uint8_t>((v >> 16) & 0xFF));
        u8(static_cast<std::uint8_t>((v >> 8) & 0xFF));
        u8(static_cast<std::uint8_t>(v & 0xFF));
    }

    void bytes(ByteSpan source) { m_buffer.insert(m_buffer.end(), source.begin(), source.end()); }
    void bytes(std::span<const std::uint8_t> source) { bytes(std::as_bytes(source)); }
    void text(std::string_view value) { bytes(asBytes(value)); }

    void fill(std::size_t count, std::uint8_t value) { m_buffer.insert(m_buffer.end(), count, std::byte{value}); }
    void zeros(std::size_t count) { fill(count, 0); }

    void padTo(std::size_t target)
    {
        if (m_buffer.size() < target)
            zeros(target - m_buffer.size());
    }

    void patchU8(std::size_t at, std::uint8_t v)
    {
        if (at < m_buffer.size())
            m_buffer[at] = std::byte{v};
    }
    void patchU16(std::size_t at, std::uint16_t v)
    {
        patchU8(at, static_cast<std::uint8_t>(v & 0xFF));
        patchU8(at + 1, static_cast<std::uint8_t>(v >> 8));
    }
    void patchU32(std::size_t at, std::uint32_t v)
    {
        patchU8(at, static_cast<std::uint8_t>(v & 0xFF));
        patchU8(at + 1, static_cast<std::uint8_t>((v >> 8) & 0xFF));
        patchU8(at + 2, static_cast<std::uint8_t>((v >> 16) & 0xFF));
        patchU8(at + 3, static_cast<std::uint8_t>(v >> 24));
    }

private:
    Bytes m_buffer;
};

} // namespace OFS::Formats
