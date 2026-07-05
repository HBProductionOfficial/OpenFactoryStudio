#include "formats/util/Crc.h"

#include <catch2/catch_test_macros.hpp>

#include <string_view>

using namespace OFS::Formats;

namespace {

std::span<const std::byte> bytes(std::string_view s)
{
    return std::as_bytes(std::span(s.data(), s.size()));
}

} // namespace

TEST_CASE("crc32 matches the standard check values", "[crc]")
{
    CHECK(crc32(bytes("")) == 0x00000000u);
    CHECK(crc32(bytes("123456789")) == 0xCBF43926u);
    CHECK(crc32(bytes("The quick brown fox jumps over the lazy dog")) == 0x414FA339u);
}

TEST_CASE("crc32 can be computed in chunks", "[crc]")
{
    const std::uint32_t whole = crc32(bytes("123456789"));
    const std::uint32_t part = crc32(bytes("6789"), crc32(bytes("12345")));
    CHECK(part == whole);
}

TEST_CASE("crc64 matches the ECMA-182 (XZ) check value", "[crc]")
{
    CHECK(crc64(bytes("")) == 0x0000000000000000ull);
    CHECK(crc64(bytes("123456789")) == 0x995DC9BBDF1939FAull);
}
