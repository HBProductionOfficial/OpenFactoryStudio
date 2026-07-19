#include "formats/util/ByteReader.h"
#include "formats/util/ByteWriter.h"
#include "formats/util/FileIo.h"
#include "formats/util/Result.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>

using namespace OFS::Formats;

namespace {

Result<int> parsePositive(int v)
{
    if (v <= 0)
        return Error{"not positive"};
    return v;
}

Status check(bool ok)
{
    if (!ok)
        return Error{"failed"};
    return {};
}

} // namespace

TEST_CASE("Result holds either a value or an error", "[util][result]")
{
    auto good = parsePositive(5);
    REQUIRE(good.ok());
    CHECK(*good == 5);
    CHECK(good.valueOr(1) == 5);

    auto bad = parsePositive(-1);
    REQUIRE_FALSE(bad);
    CHECK(bad.error().message == "not positive");
    CHECK(bad.valueOr(7) == 7);

    CHECK(check(true).ok());
    CHECK(check(false).error().message == "failed");
    CHECK(withContext("sprite 3", Error{"bad"}).message == "sprite 3: bad");

    Result<std::string> moved = std::string("text");
    const std::string taken = std::move(moved).value();
    CHECK(taken == "text");
}

TEST_CASE("ByteReader reads little- and big-endian values", "[util][bytes]")
{
    const Bytes data = {std::byte{0x01}, std::byte{0x02}, std::byte{0x03}, std::byte{0x04},
                        std::byte{0xFF}, std::byte{0xFE}, std::byte{0x80}};
    ByteReader r(data);
    CHECK(r.u16() == 0x0201);
    CHECK(r.position() == 2);
    r.seek(0);
    CHECK(r.u32() == 0x04030201u);
    CHECK(r.s16() == -257);
    CHECK(r.s8() == -128);
    CHECK(r.atEnd());
    CHECK(r.ok());
    r.seek(0);
    CHECK(r.u16be() == 0x0102);
    CHECK(r.u32be() == 0x0304FFFEu);
}

TEST_CASE("ByteReader reports overflow instead of reading past the end", "[util][bytes]")
{
    const Bytes data = {std::byte{1}, std::byte{2}, std::byte{3}};
    ByteReader r(data);
    CHECK(r.u16() == 0x0201);
    CHECK(r.u32() == 0);
    CHECK(r.overflowed());
    CHECK(r.position() == 2);
    CHECK(r.u8() == 3); // the sticky flag stays set but reads that fit still work
    CHECK_FALSE(r.ok());

    ByteReader s(data);
    CHECK(s.bytes(4).empty());
    CHECK_FALSE(s.ok());

    ByteReader t(data);
    CHECK_FALSE(t.seek(4));
    CHECK(t.seek(3));
    CHECK(t.atEnd());
    CHECK_FALSE(t.skip(1));
    CHECK(t.rest().empty());

    std::byte out[2];
    ByteReader u(ByteSpan(data).first(1));
    CHECK_FALSE(u.read(out));

    ByteReader empty;
    CHECK(empty.u8() == 0);
    CHECK(empty.overflowed());
}

TEST_CASE("ByteWriter grows and patches", "[util][bytes]")
{
    ByteWriter w;
    w.u8(0xAB);
    w.u16(0x1234);
    w.u32(0xDEADBEEF);
    w.s16(-2);
    w.u16be(0x0102);
    w.u32be(0x03040506);
    w.text("ok");
    w.zeros(2);
    w.patchU16(1, 0xCAFE);
    w.patchU32(100, 1);
    const Bytes& d = w.data();
    REQUIRE(d.size() == 1 + 2 + 4 + 2 + 2 + 4 + 2 + 2);
    ByteReader r(d);
    CHECK(r.u8() == 0xAB);
    CHECK(r.u16() == 0xCAFE);
    CHECK(r.u32() == 0xDEADBEEFu);
    CHECK(r.s16() == -2);
    CHECK(r.u16be() == 0x0102);
    CHECK(r.u32be() == 0x03040506u);
    CHECK(toText(r.bytes(2)) == "ok");
    w.padTo(30);
    CHECK(w.size() == 30);
    const Bytes taken = w.take();
    CHECK(taken.size() == 30);
    CHECK(w.size() == 0);
}

TEST_CASE("readFile and writeFile round-trip", "[util][file]")
{
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "ofs-formats-test";
    std::filesystem::create_directories(dir);
    const std::filesystem::path file = dir / "roundtrip.bin";
    const Bytes data = toBytes(std::string_view("binary\0data", 11));
    REQUIRE(writeFile(file, data).ok());
    REQUIRE(writeFile(file, data).ok());
    auto back = readFile(file);
    REQUIRE(back.ok());
    CHECK(*back == data);
    std::filesystem::remove(file);
    CHECK_FALSE(readFile(dir / "missing.bin").ok());
}
