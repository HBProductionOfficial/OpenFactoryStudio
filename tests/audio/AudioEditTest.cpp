#include "formats/audio/AudioEdit.h"
#include "audio/AudioTestUtil.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>

using namespace OFS::Formats;
using namespace OFS::Formats::Test;

using Values = std::vector<std::int16_t>;

namespace {

const SampleBuffer& eight()
{
    static const SampleBuffer buffer = int16Buffer({10, 20, 30, 40, 50, 60, 70, 80});
    return buffer;
}

SampleBuffer constant(std::size_t frames, std::int16_t value)
{
    return SampleBuffer::fromInt16(Values(frames, value), 1, 22050);
}

template <typename T>
T ok(Result<T> result)
{
    if (!result)
        FAIL(result.error().message);
    return std::move(*result);
}

} // namespace

TEST_CASE("Copy, remove and trim take sample-exact ranges", "[audio][edit]")
{
    CHECK(int16Values(copyRange(eight(), {2, 3})) == Values{30, 40, 50});
    CHECK(int16Values(copyRange(eight(), {6, 100})) == Values{70, 80});
    CHECK(copyRange(eight(), {100, 5}).empty());
    CHECK(copyRange(eight(), kAllFrames) == eight());
    CHECK(copyRange(eight(), SampleRange::between(5, 3)) == copyRange(eight(), {3, 2}));

    CHECK(int16Values(removeRange(eight(), {2, 3})) == Values{10, 20, 60, 70, 80});
    CHECK(int16Values(removeRange(eight(), {0, 1})) == Values{20, 30, 40, 50, 60, 70, 80});
    CHECK(int16Values(removeRange(eight(), {7, 5})) == Values{10, 20, 30, 40, 50, 60, 70});
    CHECK(removeRange(eight(), {3, 0}) == eight());
    CHECK(removeRange(eight(), kAllFrames).empty());

    CHECK(int16Values(trim(eight(), {1, 2})) == Values{20, 30});

    const SampleBuffer stereo = int16Buffer({1, 2, 3, 4, 5, 6}, 2, 44100);
    const SampleBuffer middle = copyRange(stereo, {1, 1});
    CHECK(int16Values(middle) == Values{3, 4});
    CHECK(middle.sameFormat(stereo));
    CHECK(int16Values(removeRange(stereo, {1, 1})) == Values{1, 2, 5, 6});

    CHECK(floatValues(removeRange(floatBuffer({0.1f, 0.2f, 0.3f}), {1, 1})) == std::vector<float>{0.1f, 0.3f});
}

TEST_CASE("Paste inserts or replaces, converting the clip first", "[audio][edit]")
{
    const SampleBuffer clip = int16Buffer({1, 2});
    CHECK(int16Values(ok(insertBuffer(eight(), 2, clip))) == Values{10, 20, 1, 2, 30, 40, 50, 60, 70, 80});
    CHECK(int16Values(ok(insertBuffer(eight(), 0, clip))) == Values{1, 2, 10, 20, 30, 40, 50, 60, 70, 80});
    CHECK(int16Values(ok(insertBuffer(eight(), 99, clip))) == Values{10, 20, 30, 40, 50, 60, 70, 80, 1, 2});
    CHECK(int16Values(ok(replaceRange(eight(), {1, 3}, int16Buffer({9})))) == Values{10, 9, 50, 60, 70, 80});
    CHECK(int16Values(ok(replaceRange(eight(), {6, 9}, clip))) == Values{10, 20, 30, 40, 50, 60, 1, 2});

    const SampleBuffer stereoClip = floatBuffer({0.5f, 0.0f, -1.0f, -1.0f}, 2);
    CHECK(int16Values(ok(insertBuffer(int16Buffer({7}), 1, stereoClip))) == Values{7, 8192, -32768});

    const SampleBuffer fast = sine(SampleType::Float32, 2, 44100, 4410, 1000.0);
    const SampleBuffer target(SampleType::Int16, 1, 22050, 100);
    const SampleBuffer conformed = ok(conformTo(fast, target));
    CHECK(conformed.sameFormat(target));
    CHECK(conformed.frameCount() == 2205);
    CHECK(ok(insertBuffer(target, 50, fast)).frameCount() == 2305);
}

TEST_CASE("Silence can be inserted or filled in", "[audio][edit]")
{
    CHECK(int16Values(insertSilence(eight(), 3, 2)) == Values{10, 20, 30, 0, 0, 40, 50, 60, 70, 80});
    CHECK(int16Values(insertSilence(eight(), 50, 1)) == Values{10, 20, 30, 40, 50, 60, 70, 80, 0});
    CHECK(insertSilence(eight(), 3, 0) == eight());
    CHECK(int16Values(insertSilence(int16Buffer({1, 2}, 2), 0, 1)) == Values{0, 0, 1, 2});

    CHECK(int16Values(fillSilence(eight(), {1, 2})) == Values{10, 0, 0, 40, 50, 60, 70, 80});
    CHECK(int16Values(fillSilence(eight(), {6, 10})) == Values{10, 20, 30, 40, 50, 60, 0, 0});
    const SampleBuffer floats =
        fillSilence(floatBuffer({0.5f, std::numeric_limits<float>::quiet_NaN(), 0.25f}), {1, 1});
    CHECK(floatValues(floats) == std::vector<float>{0.5f, 0.0f, 0.25f});
}

TEST_CASE("Fades are linear ramps over the selection", "[audio][edit]")
{
    const SampleBuffer level = constant(8, 1000);
    CHECK(int16Values(fadeIn(level, {0, 5})) == Values{0, 250, 500, 750, 1000, 1000, 1000, 1000});
    CHECK(int16Values(fadeOut(level, {0, 5})) == Values{1000, 750, 500, 250, 0, 1000, 1000, 1000});
    CHECK(int16Values(fadeOut(level, {3, 5})) == Values{1000, 1000, 1000, 1000, 750, 500, 250, 0});
    CHECK(fadeIn(level, {2, 1}) == level);
    CHECK(int16Values(fadeOut(level, {2, 1})) == Values{1000, 1000, 0, 1000, 1000, 1000, 1000, 1000});
    CHECK(fadeIn(level, {8, 4}) == level);

    const SampleBuffer stereo = floatBuffer({1.0f, -1.0f, 1.0f, -1.0f, 1.0f, -1.0f}, 2);
    CHECK(floatValues(fadeIn(stereo, kAllFrames)) == std::vector<float>{0.0f, -0.0f, 0.5f, -0.5f, 1.0f, -1.0f});
}

TEST_CASE("Volume changes and normalisation round and clamp", "[audio][edit]")
{
    CHECK(int16Values(changeVolume(eight(), kAllFrames, 0.5f)) == Values{5, 10, 15, 20, 25, 30, 35, 40});
    CHECK(int16Values(changeVolume(int16Buffer({3, -3}), kAllFrames, 0.5f)) == Values{2, -2});
    CHECK(int16Values(changeVolume(int16Buffer({30000, -30000, 100}), {0, 2}, 2.0f)) == Values{32767, -32768, 100});
    CHECK(changeVolume(eight(), kAllFrames, 1.0f) == eight());

    const SampleBuffer quiet = int16Buffer({1000, -2000, 500, 4000});
    CHECK(int16Values(normalize(quiet, {0, 3})) == Values{16384, -32768, 8192, 4000});
    CHECK(int16Values(normalize(quiet, {0, 3}, 0.5f)) == Values{8192, -16384, 4096, 4000});
    CHECK(floatValues(normalize(floatBuffer({0.25f, -0.5f}), kAllFrames)) == std::vector<float>{0.5f, -1.0f});
    CHECK(normalize(constant(4, 0), kAllFrames) == constant(4, 0));

    CHECK(peakLevel(int16Buffer({5, -32768, 7})) == 1.0f);
    CHECK(peakLevel(floatBuffer({0.25f, -0.75f, 0.5f})) == 0.75f);
    CHECK(peakLevel(floatBuffer({0.25f, -0.75f, 0.5f}), {2, 1}) == 0.5f);
    CHECK(peakLevel(eight(), {8, 1}) == 0.0f);

    CHECK(decibelsToGain(0.0f) == 1.0f);
    CHECK(decibelsToGain(-20.0f) == Catch::Approx(0.1f));
    CHECK(gainToDecibels(0.5f) == Catch::Approx(-6.0206f).epsilon(1e-4));
    CHECK(std::isinf(gainToDecibels(0.0f)));
}

TEST_CASE("Reverse and move rearrange frames", "[audio][edit]")
{
    CHECK(int16Values(reverse(eight(), {1, 4})) == Values{10, 50, 40, 30, 20, 60, 70, 80});
    CHECK(int16Values(reverse(eight(), kAllFrames)) == Values{80, 70, 60, 50, 40, 30, 20, 10});
    CHECK(int16Values(reverse(int16Buffer({1, 2, 3, 4, 5, 6}, 2), kAllFrames)) == Values{5, 6, 3, 4, 1, 2});
    CHECK(reverse(eight(), {3, 1}) == eight());

    const SampleBuffer six = int16Buffer({10, 20, 30, 40, 50, 60});
    CHECK(int16Values(moveRange(six, {0, 2}, 3)) == Values{30, 40, 50, 10, 20, 60});
    CHECK(int16Values(moveRange(six, {0, 2}, 100)) == Values{30, 40, 50, 60, 10, 20});
    CHECK(int16Values(moveRange(six, {4, 2}, 0)) == Values{50, 60, 10, 20, 30, 40});
    CHECK(moveRange(six, {2, 2}, 2) == six);
}

TEST_CASE("Stretching a selection resamples only that part", "[audio][edit]")
{
    const SampleBuffer tone = sine(SampleType::Int16, 1, 22050, 3000, 500.0);
    const SampleBuffer stretched = ok(stretchRange(tone, {1000, 1000}, 2000));
    REQUIRE(stretched.frameCount() == 4000);
    CHECK(copyRange(stretched, {0, 1000}) == copyRange(tone, {0, 1000}));
    CHECK(copyRange(stretched, {3000, 1000}) == copyRange(tone, {2000, 1000}));
    CHECK(std::fabs(toneLevel(stretched, 0, 250.0, 1200, 1600) - 0.5) < 0.02);
    CHECK(ok(stretchRange(tone, {500, 100}, 100)) == tone);
}

TEST_CASE("Silence at the ends can be found and trimmed", "[audio][edit]")
{
    const SampleBuffer padded = int16Buffer({0, 0, 5, 0, 7, 0, 0});
    CHECK(nonSilentRange(padded) == SampleRange{2, 3});
    CHECK(nonSilentRange(padded, 6.0f / 32768.0f) == SampleRange{4, 1});
    CHECK(nonSilentRange(constant(5, 0)) == SampleRange{0, 0});
    CHECK(int16Values(trimSilence(padded)) == Values{5, 0, 7});
    CHECK(trimSilence(constant(5, 3), 4.0f / 32768.0f).empty());
    CHECK(nonSilentRange(int16Buffer({0, 0, 0, 9, 0, 0}, 2)) == SampleRange{1, 1});
}

TEST_CASE("Edits never change their input", "[audio][edit]")
{
    const SampleBuffer before = eight();
    SampleBuffer input = eight();
    (void)removeRange(input, {1, 2});
    (void)fadeIn(input, kAllFrames);
    (void)normalize(input, kAllFrames);
    (void)reverse(input, kAllFrames);
    (void)moveRange(input, {0, 3}, 4);
    (void)insertSilence(input, 1, 3);
    (void)fillSilence(input, kAllFrames);
    CHECK(input == before);
}
