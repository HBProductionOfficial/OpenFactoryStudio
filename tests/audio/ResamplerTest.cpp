#include "formats/audio/Resampler.h"
#include "audio/AudioTestUtil.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>

using namespace OFS::Formats;
using namespace OFS::Formats::Test;

namespace {

SampleBuffer resampled(const SampleBuffer& buffer, std::uint32_t rate, ResampleQuality quality = ResampleQuality::Best)
{
    auto out = resample(buffer, rate, quality);
    if (!out)
        FAIL(out.error().message);
    return std::move(*out);
}

} // namespace

TEST_CASE("Resampling keeps the frequency and the timing", "[audio][resample]")
{
    struct Case
    {
        ResampleQuality quality;
        double tolerance;
    };
    for (const Case& c : {Case{ResampleQuality::Best, 0.002}, Case{ResampleQuality::Medium, 0.002},
                          Case{ResampleQuality::Fastest, 0.005}, Case{ResampleQuality::Linear, 0.05}}) {
        const SampleBuffer up = resampled(sine(SampleType::Float32, 1, 22050, 22050, 1000.0), 44100, c.quality);
        CHECK(up.sampleRate() == 44100);
        CHECK(up.frameCount() == 44100);
        const SampleBuffer ideal = sine(SampleType::Float32, 1, 44100, 44100, 1000.0);
        CHECK(maxDifference(up, ideal, 4410, 35280) < c.tolerance);

        const SampleBuffer down = resampled(sine(SampleType::Float32, 1, 48000, 48000, 1000.0), 16000, c.quality);
        CHECK(down.frameCount() == 16000);
        CHECK(maxDifference(down, sine(SampleType::Float32, 1, 16000, 16000, 1000.0), 1600, 12800) < c.tolerance);
        CHECK(std::fabs(toneLevel(down, 0, 1000.0, 1600, 12800) - 0.5) < 0.01);
    }
}

TEST_CASE("The sinc converters filter out what the new rate cannot hold", "[audio][resample]")
{
    // 9 kHz is above the 5.5 kHz Nyquist limit. Band-limited converters remove it, sample-and-hold
    // aliases it to 2.025 kHz.
    const SampleBuffer high = sine(SampleType::Float32, 1, 44100, 44100, 9000.0);
    const SampleBuffer best = resampled(high, 11025, ResampleQuality::Best);
    CHECK(rms(best, 0, 1100, 8800) < 0.002);
    const SampleBuffer hold = resampled(high, 11025, ResampleQuality::ZeroOrderHold);
    CHECK(rms(hold, 0, 1100, 8800) > 0.1);
    CHECK(toneLevel(hold, 0, 2025.0, 1100, 8800) > 0.1);
}

TEST_CASE("Resampled lengths are rounded exactly", "[audio][resample]")
{
    struct Rates
    {
        std::uint32_t from;
        std::uint32_t to;
    };
    for (const Rates r : {Rates{22050, 17700}, Rates{11025, 44100}, Rates{44100, 8000}, Rates{48000, 44100}}) {
        for (const std::size_t frames : {std::size_t{1}, std::size_t{2}, std::size_t{7}, std::size_t{100},
                                         std::size_t{1001}, std::size_t{22050}}) {
            const SampleBuffer out = resampled(noise(SampleType::Int16, 1, r.from, frames, frames), r.to);
            const auto expected = static_cast<std::size_t>(
                std::llround(static_cast<double>(frames) * r.to / static_cast<double>(r.from)));
            CHECK(out.frameCount() == expected);
        }
    }
}

TEST_CASE("Resampling keeps the sample type and the channels apart", "[audio][resample]")
{
    const SampleBuffer stereo = sine(SampleType::Int16, 2, 22050, 22050, 500.0);
    const SampleBuffer out = resampled(stereo, 32000);
    CHECK(out.type() == SampleType::Int16);
    CHECK(out.channels() == 2);
    CHECK(out.frameCount() == 32000);
    CHECK(std::fabs(toneLevel(out, 0, 500.0, 3200, 25600) - 0.5) < 0.01);
    CHECK(std::fabs(toneLevel(out, 1, 1000.0, 3200, 25600) - 0.5) < 0.01);
    CHECK(toneLevel(out, 0, 1000.0, 3200, 25600) < 0.001);
    CHECK(toneLevel(out, 1, 500.0, 3200, 25600) < 0.001);

    SampleBuffer level(SampleType::Float32, 1, 8000, 4000);
    for (std::size_t f = 0; f < level.frameCount(); ++f)
        level.setSample(f, 0, 0.5f);
    for (const ResampleQuality q : {ResampleQuality::Best, ResampleQuality::Medium, ResampleQuality::Fastest,
                                    ResampleQuality::Linear, ResampleQuality::ZeroOrderHold}) {
        const SampleBuffer up = resampled(level, 11025, q);
        for (std::size_t f = 1000; f < 4000; f += 250)
            CHECK(std::fabs(up.sample(f, 0) - 0.5f) < 0.001f);
    }
}

TEST_CASE("Linear and zero-order-hold resampling stay aligned with the input", "[audio][resample]")
{
    const SampleBuffer steps = int16Buffer({0, 100, 200}, 1, 1000);
    CHECK(int16Values(resampled(steps, 2000, ResampleQuality::Linear)) ==
          std::vector<std::int16_t>{0, 50, 100, 150, 200, 200});
    CHECK(int16Values(resampled(steps, 2000, ResampleQuality::ZeroOrderHold)) ==
          std::vector<std::int16_t>{0, 0, 100, 100, 200, 200});
    CHECK(int16Values(resampled(int16Buffer({0, 1, 2, 3, 4, 5}, 1, 3000), 1000, ResampleQuality::Linear)) ==
          std::vector<std::int16_t>{0, 3});
}

TEST_CASE("Resampling edge cases", "[audio][resample]")
{
    const SampleBuffer buffer = noise(SampleType::Float32, 2, 22050, 500, 11);
    CHECK(resampled(buffer, 22050) == buffer);

    const SampleBuffer empty = resampled(SampleBuffer(SampleType::Int16, 2, 22050), 44100);
    CHECK(empty.empty());
    CHECK(empty.sampleRate() == 44100);
    CHECK(empty.channels() == 2);

    CHECK_FALSE(resample(buffer, 0).ok());
    SampleBuffer noRate = buffer;
    noRate.setSampleRate(0);
    CHECK_FALSE(resample(noRate, 44100).ok());
    CHECK_FALSE(resample(SampleBuffer(SampleType::Float32, 1, 1000, 100), 300000).ok()); // ratio above 256
}

TEST_CASE("Stretching to a length changes the pitch, not the rate", "[audio][resample]")
{
    const SampleBuffer tone = sine(SampleType::Float32, 1, 44100, 4410, 1000.0);
    auto longer = resampleToLength(tone, 8820);
    REQUIRE(longer.ok());
    CHECK(longer->sampleRate() == 44100);
    CHECK(longer->frameCount() == 8820);
    CHECK(std::fabs(toneLevel(*longer, 0, 500.0, 882, 7056) - 0.5) < 0.01);

    auto shorter = resampleToLength(tone, 2205);
    REQUIRE(shorter.ok());
    CHECK(shorter->frameCount() == 2205);
    CHECK(std::fabs(toneLevel(*shorter, 0, 2000.0, 220, 1764) - 0.5) < 0.01);

    CHECK(*resampleToLength(tone, 4410) == tone);
    auto fromNothing = resampleToLength(SampleBuffer(SampleType::Int16, 1, 8000), 10);
    REQUIRE(fromNothing.ok());
    CHECK(fromNothing->frameCount() == 10);
    CHECK(int16Values(*fromNothing) == std::vector<std::int16_t>(10, 0));
    CHECK(resampleToLength(tone, 0)->empty());
    CHECK_FALSE(resampleToLength(int16Buffer({1, 2}), 1000).ok()); // ratio above 256
}
