#include "formats/audio/SampleBuffer.h"
#include "audio/AudioTestUtil.h"
#include "formats/util/ByteReader.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>

using namespace OFS::Formats;
using namespace OFS::Formats::Test;

TEST_CASE("Sample buffers hold interleaved frames of either sample type", "[audio][buffer]")
{
    SampleBuffer silent(SampleType::Float32, 2, 44100, 10);
    CHECK(silent.type() == SampleType::Float32);
    CHECK(silent.channels() == 2);
    CHECK(silent.sampleRate() == 44100);
    CHECK(silent.frameCount() == 10);
    CHECK(silent.sampleCount() == 20);
    CHECK(silent.byteSize() == 80);
    CHECK(silent.int16Samples().empty());
    CHECK(silent.sample(9, 1) == 0.0f);

    SampleBuffer none(SampleType::Int16, 0, 8000);
    CHECK(none.channels() == 1);
    CHECK(none.empty());
    CHECK(none.durationSeconds() == 0.0);

    const SampleBuffer odd = SampleBuffer::fromInt16({1, 2, 3, 4, 5}, 2, 22050);
    CHECK(odd.frameCount() == 2);
    CHECK(int16Values(odd) == std::vector<std::int16_t>{1, 2, 3, 4});

    SampleBuffer grown = odd;
    grown.resize(3);
    CHECK(int16Values(grown) == std::vector<std::int16_t>{1, 2, 3, 4, 0, 0});
    CHECK(grown.sameFormat(odd));
    CHECK_FALSE(grown == odd);
    grown.setSampleRate(11025);
    CHECK_FALSE(grown.sameFormat(odd));
    CHECK(SampleBuffer(SampleType::Int16, 1, 22050, 22050).durationSeconds() == 1.0);
}

TEST_CASE("Int16 samples convert to floats and back without loss", "[audio][buffer]")
{
    CHECK(int16ToFloat(-32768) == -1.0f);
    CHECK(int16ToFloat(16384) == 0.5f);
    for (int s = -32768; s <= 32767; ++s) {
        const auto value = static_cast<std::int16_t>(s);
        if (floatToInt16(int16ToFloat(value)) != value)
            FAIL("sample " << s << " does not survive the round trip");
    }
    CHECK(floatToInt16(0.5f / 32768.0f) == 1);
    CHECK(floatToInt16(-0.5f / 32768.0f) == -1);
    CHECK(floatToInt16(0.49f / 32768.0f) == 0);
    CHECK(floatToInt16(2.5f / 32768.0f) == 3);
    CHECK(floatToInt16(1.0f) == 32767);
    CHECK(floatToInt16(-1.0f) == -32768);
    CHECK(floatToInt16(7.0f) == 32767);
    CHECK(floatToInt16(-7.0f) == -32768);
    CHECK(floatToInt16(std::numeric_limits<float>::quiet_NaN()) == 0);
    CHECK(floatToInt16(std::numeric_limits<float>::infinity()) == 32767);

    SampleBuffer buffer(SampleType::Int16, 1, 8000, 3);
    buffer.setSample(0, 0, 0.25f);
    buffer.setSample(1, 0, 1.5f);
    buffer.setSample(2, 0, -0.25f);
    buffer.setSample(3, 0, 1.0f);
    buffer.setSample(0, 1, 1.0f);
    CHECK(int16Values(buffer) == std::vector<std::int16_t>{8192, 32767, -8192});
    CHECK(buffer.sample(1, 0) == 32767.0f / 32768.0f);
    CHECK(buffer.sample(5, 0) == 0.0f);

    const SampleBuffer asFloat = convertSampleType(buffer, SampleType::Float32);
    CHECK(floatValues(asFloat) == std::vector<float>{0.25f, 32767.0f / 32768.0f, -0.25f});
    CHECK(convertSampleType(asFloat, SampleType::Int16) == buffer);
    CHECK(convertSampleType(buffer, SampleType::Int16) == buffer);
}

TEST_CASE("Channel conversion duplicates mono and averages to mono", "[audio][buffer]")
{
    const SampleBuffer mono = int16Buffer({100, -200, 32767});
    const SampleBuffer stereo = toStereo(mono);
    CHECK(stereo.channels() == 2);
    CHECK(int16Values(stereo) == std::vector<std::int16_t>{100, 100, -200, -200, 32767, 32767});
    CHECK(toMono(stereo) == mono);

    const SampleBuffer pairs = int16Buffer({1, 2, -1, -2, 32767, 32766, -32768, -32768}, 2);
    CHECK(int16Values(toMono(pairs)) == std::vector<std::int16_t>{2, -2, 32767, -32768});
    CHECK(floatValues(toMono(floatBuffer({0.5f, 0.25f, -1.0f, 1.0f}, 2))) == std::vector<float>{0.375f, 0.0f});

    const SampleBuffer three = int16Buffer({1, 2, 3, 4, 5, 6}, 3);
    CHECK(int16Values(convertChannels(three, 2)) == std::vector<std::int16_t>{1, 2, 4, 5});
    CHECK(int16Values(convertChannels(int16Buffer({1, 2, 3, 4}, 2), 3)) == std::vector<std::int16_t>{1, 2, 0, 3, 4, 0});
    CHECK(convertChannels(mono, 1) == mono);
    CHECK(convertChannels(mono, 0) == mono);
}

TEST_CASE("Quantising rounds to the grid of the target depth and clamps", "[audio][buffer]")
{
    const SampleBuffer samples = int16Buffer({127, 128, -128, -129, 32767, -32768, 1000});
    auto eight = quantize(samples, 8);
    REQUIRE(eight.ok());
    CHECK(int16Values(*eight) == std::vector<std::int16_t>{0, 256, -256, -256, 32512, -32768, 1024});
    CHECK(*quantize(samples, 16) == samples);
    CHECK(*quantize(samples, 24) == samples);

    const float lsb16 = 1.0f / 32768.0f;
    auto floats = quantize(floatBuffer({0.5f * lsb16, 0.49f * lsb16, -0.5f * lsb16, 1.5f, -1.5f, 0.25f}), 16);
    REQUIRE(floats.ok());
    CHECK(floatValues(*floats) == std::vector<float>{lsb16, 0.0f, -lsb16, 32767.0f / 32768.0f, -1.0f, 0.25f});
    auto deep = quantize(floatBuffer({0.3f}), 24);
    REQUIRE(deep.ok());
    CHECK(deep->floatSamples()[0] == static_cast<float>(std::round(0.3f * 8388608.0) / 8388608.0));

    CHECK_FALSE(quantize(samples, 12).ok());
    CHECK_FALSE(quantize(samples, 0).ok());
}

TEST_CASE("WAV files convert to sample buffers and back exactly", "[audio][buffer][wav]")
{
    SECTION("8-bit")
    {
        std::vector<std::int16_t> values;
        for (int v = -128; v <= 127; ++v)
            values.push_back(static_cast<std::int16_t>(v * 256));
        const SampleBuffer buffer = SampleBuffer::fromInt16(values, 2, 11025);
        auto wav = buffer.toWav({8, false});
        REQUIRE(wav.ok());
        CHECK(wav->format.formatTag == kWavFormatPcm);
        CHECK(wav->format.blockAlign == 2);
        CHECK(wav->format.byteRate == 22050);
        // 8-bit WAV samples are unsigned: -32768 is 0, silence is 128.
        CHECK(static_cast<std::uint8_t>(wav->samples[0]) == 0);
        CHECK(static_cast<std::uint8_t>(wav->samples[128]) == 128);
        auto parsed = WavFile::parse(wav->write());
        REQUIRE(parsed.ok());
        auto back = SampleBuffer::fromWav(*parsed);
        REQUIRE(back.ok());
        CHECK(back->type() == SampleType::Int16);
        CHECK(*back == buffer);
    }
    SECTION("16-bit")
    {
        const SampleBuffer buffer = noise(SampleType::Int16, 2, 22050, 3000, 1);
        auto wav = buffer.toWav();
        REQUIRE(wav.ok());
        CHECK(wav->format.bitsPerSample == 16);
        auto back = SampleBuffer::fromWav(*WavFile::parse(wav->write()));
        REQUIRE(back.ok());
        CHECK(*back == buffer);
        CHECK(static_cast<float>(wav->sample(17, 1)) == buffer.sample(17, 1));
    }
    SECTION("24-bit and 32-bit")
    {
        for (const std::uint16_t bits : {std::uint16_t{24}, std::uint16_t{32}}) {
            SampleBuffer buffer(SampleType::Float32, 1, 48000, 512);
            for (std::size_t i = 0; i < 512; ++i) {
                const int step = static_cast<int>(i * 32749 % 16777216) - 8388608;
                buffer.setSample(i, 0, static_cast<float>(step) / 8388608.0f);
            }
            auto wav = buffer.toWav({bits, false});
            REQUIRE(wav.ok());
            auto back = SampleBuffer::fromWav(*WavFile::parse(wav->write()));
            REQUIRE(back.ok());
            CHECK(back->type() == SampleType::Float32);
            CHECK(*back == buffer);
        }
    }
    SECTION("floating point")
    {
        const SampleBuffer buffer = floatBuffer({0.1f, -0.7f, 1.25f, -3.0f});
        for (const std::uint16_t bits : {std::uint16_t{32}, std::uint16_t{64}}) {
            auto wav = buffer.toWav({bits, true});
            REQUIRE(wav.ok());
            CHECK(wav->format.formatTag == kWavFormatFloat);
            auto back = SampleBuffer::fromWav(*WavFile::parse(wav->write()));
            REQUIRE(back.ok());
            CHECK(*back == buffer);
        }
    }
    SECTION("depth changes round and clamp")
    {
        auto wav = floatBuffer({0.5f / 32768.0f, 2.0f, -2.0f, 0.25f}).toWav({16, false});
        REQUIRE(wav.ok());
        ByteReader r(wav->samples);
        CHECK(r.s16() == 1);
        CHECK(r.s16() == 32767);
        CHECK(r.s16() == -32768);
        CHECK(r.s16() == 8192);

        const SampleBuffer ints = int16Buffer({1, -1, 32767});
        auto wide = SampleBuffer::fromWav(*ints.toWav({24, false}), SampleType::Int16);
        REQUIRE(wide.ok());
        CHECK(*wide == ints);
        auto narrow = SampleBuffer::fromWav(*ints.toWav({8, false}));
        REQUIRE(narrow.ok());
        CHECK(int16Values(*narrow) == std::vector<std::int16_t>{0, 0, 32512});
    }
    SECTION("a forced sample type")
    {
        const SampleBuffer ints = int16Buffer({-32768, 0, 16384});
        auto asFloat = SampleBuffer::fromWav(*ints.toWav(), SampleType::Float32);
        REQUIRE(asFloat.ok());
        CHECK(floatValues(*asFloat) == std::vector<float>{-1.0f, 0.0f, 0.5f});
        auto fromFloatWav = SampleBuffer::fromWav(*asFloat->toWav({32, true}), SampleType::Int16);
        REQUIRE(fromFloatWav.ok());
        CHECK(*fromFloatWav == ints);
    }
}

TEST_CASE("Unsupported WAV conversions are reported", "[audio][buffer][wav]")
{
    const SampleBuffer buffer = int16Buffer({1, 2});
    CHECK_FALSE(buffer.toWav({12, false}).ok());
    CHECK_FALSE(buffer.toWav({16, true}).ok());
    CHECK_FALSE(buffer.toWav({0, false}).ok());

    WavFile adpcm = *buffer.toWav();
    adpcm.format.formatTag = 2;
    CHECK_FALSE(SampleBuffer::fromWav(adpcm).ok());
    WavFile noRate = *buffer.toWav();
    noRate.format.sampleRate = 0;
    CHECK_FALSE(SampleBuffer::fromWav(noRate).ok());
}
