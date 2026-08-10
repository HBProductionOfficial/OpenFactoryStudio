#include "formats/audio/WaveformPeaks.h"
#include "audio/AudioTestUtil.h"
#include "formats/audio/AudioEdit.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <vector>

using namespace OFS::Formats;
using namespace OFS::Formats::Test;

namespace {

PeakPair bruteForce(const SampleBuffer& buffer, std::uint16_t channel, SampleRange range)
{
    const SampleRange r = range.clipped(buffer.frameCount());
    if (r.empty())
        return {};
    PeakPair pair{buffer.sample(r.start, channel), buffer.sample(r.start, channel)};
    for (std::size_t f = r.start; f < r.end(); ++f) {
        pair.min = std::min(pair.min, buffer.sample(f, channel));
        pair.max = std::max(pair.max, buffer.sample(f, channel));
    }
    return pair;
}

void checkRanges(const WaveformPeaks& peaks, const SampleBuffer& buffer, std::uint64_t seed)
{
    std::uint64_t state = seed;
    auto next = [&state](std::size_t bound) {
        state = state * 6364136223846793005ull + 1442695040888963407ull;
        return bound ? static_cast<std::size_t>((state >> 33) % bound) : 0;
    };
    const std::size_t frames = buffer.frameCount();
    for (int i = 0; i < 300; ++i) {
        const std::size_t start = next(frames + 10);
        const std::size_t count = next(std::size_t{1} << next(24));
        const std::uint16_t channel = static_cast<std::uint16_t>(next(buffer.channels()));
        const SampleRange range{start, count};
        if (!(peaks.peak(buffer, channel, range) == bruteForce(buffer, channel, range)))
            FAIL("range " << start << "+" << count << " of channel " << channel << " differs");
    }
}

} // namespace

TEST_CASE("Peaks give the exact extremes of any range", "[audio][peaks]")
{
    for (const SampleType type : {SampleType::Int16, SampleType::Float32}) {
        const SampleBuffer buffer = noise(type, 2, 44100, 300007, 21);
        WaveformPeaks peaks;
        peaks.build(buffer);
        CHECK(peaks.channels() == 2);
        CHECK(peaks.isComplete(buffer));
        CHECK(peaks.summarizedFrames() == 300007 / WaveformPeaks::kBlockFrames * WaveformPeaks::kBlockFrames);
        CHECK(peaks.levelCount() > 4);
        checkRanges(peaks, buffer, 1);
        CHECK(peaks.peak(buffer, 0, kAllFrames) == bruteForce(buffer, 0, kAllFrames));
        CHECK(peaks.peak(buffer, 0, {300007, 5}) == PeakPair{});
        CHECK(peaks.peak(buffer, 2, kAllFrames) == PeakPair{});
    }
}

TEST_CASE("Peaks can be built in steps and stay exact meanwhile", "[audio][peaks]")
{
    const SampleBuffer buffer = noise(SampleType::Float32, 1, 22050, 100000, 22);
    WaveformPeaks peaks;
    std::size_t steps = 0;
    while (peaks.extend(buffer, 7000) > 0) {
        ++steps;
        CHECK(peaks.summarizedFrames() <= buffer.frameCount());
        checkRanges(peaks, buffer, steps);
    }
    CHECK(steps == (100000 / 256 + 27) / 28); // 7000 frames round up to 28 blocks per step
    CHECK(peaks.isComplete(buffer));
    CHECK(peaks.extend(buffer) == 0);

    const WaveformPeaks none;
    CHECK(none.peak(buffer, 0, {123, 4567}) == bruteForce(buffer, 0, {123, 4567}));
    CHECK(none.byteSize() == 0);
}

TEST_CASE("Peaks follow edits", "[audio][peaks]")
{
    SampleBuffer buffer = noise(SampleType::Int16, 2, 22050, 50000, 23);
    WaveformPeaks peaks;
    peaks.build(buffer);

    buffer = changeVolume(buffer, {12345, 3000}, 0.1f);
    buffer = fillSilence(buffer, {40000, 10});
    peaks.update(buffer, {12345, 3000});
    peaks.update(buffer, {40000, 10});
    checkRanges(peaks, buffer, 2);

    buffer = insertSilence(buffer, 20000, 777);
    peaks.invalidateFrom(20000);
    CHECK(peaks.summarizedFrames() <= 20000);
    peaks.extend(buffer);
    CHECK(peaks.isComplete(buffer));
    checkRanges(peaks, buffer, 3);

    buffer = removeRange(buffer, {1000, 30000});
    peaks.invalidateFrom(1000);
    peaks.extend(buffer);
    checkRanges(peaks, buffer, 4);

    const SampleBuffer mono = toMono(buffer);
    peaks.extend(mono);
    CHECK(peaks.channels() == 1);
    checkRanges(peaks, mono, 5);
}

TEST_CASE("Peak columns cover the view pixel by pixel", "[audio][peaks]")
{
    SampleBuffer ramp(SampleType::Float32, 1, 8000, 1024);
    for (std::size_t f = 0; f < 1024; ++f)
        ramp.setSample(f, 0, static_cast<float>(f) / 1024.0f);
    WaveformPeaks peaks;
    peaks.build(ramp);
    auto value = [](double frame) {
        return static_cast<float>(frame / 1024.0);
    };

    std::vector<PeakPair> columns(4);
    peaks.columns(ramp, 0, 0.0, 256.0, columns);
    for (std::size_t i = 0; i < 4; ++i)
        CHECK(columns[i] == PeakPair{value(i * 256.0), value(i * 256.0 + 255)});

    std::vector<PeakPair> fine(3);
    peaks.columns(ramp, 0, 10.5, 2.5, fine);
    CHECK(fine[0] == PeakPair{value(10), value(12)});
    CHECK(fine[1] == PeakPair{value(13), value(14)});
    CHECK(fine[2] == PeakPair{value(15), value(17)});

    std::vector<PeakPair> zoomed(4);
    peaks.columns(ramp, 0, 100.0, 0.5, zoomed);
    CHECK(zoomed[0] == PeakPair{value(100), value(100)});
    CHECK(zoomed[1] == PeakPair{value(100), value(100)});
    CHECK(zoomed[2] == PeakPair{value(101), value(101)});

    std::vector<PeakPair> edges(4);
    peaks.columns(ramp, 0, -300.0, 256.0, edges);
    CHECK(edges[0] == PeakPair{});
    CHECK(edges[1] == PeakPair{value(0), value(211)});
    peaks.columns(ramp, 0, 900.0, 100.0, edges);
    CHECK(edges[0] == PeakPair{value(900), value(999)});
    CHECK(edges[1] == PeakPair{value(1000), value(1023)});
    CHECK(edges[2] == PeakPair{});
    peaks.columns(ramp, 0, 0.0, 0.0, edges);
    CHECK(edges[0] == PeakPair{});
}

TEST_CASE("A ten-minute track is summarised compactly and drawn exactly", "[audio][peaks]")
{
    const std::size_t frames = 10 * 60 * 44100;
    SampleBuffer track(SampleType::Int16, 2, 44100, frames);
    std::span<std::int16_t> samples = track.int16Samples();
    for (std::size_t f = 0; f < frames; ++f) {
        const auto v = static_cast<std::int16_t>(static_cast<int>(f % 100) * 40 - 2000);
        samples[f * 2] = v;
        samples[f * 2 + 1] = static_cast<std::int16_t>(-v);
    }
    for (const std::size_t click : {std::size_t{1234567}, std::size_t{9876543}, frames - 1})
        samples[click * 2] = 30000;

    WaveformPeaks peaks;
    peaks.build(track);
    CHECK(peaks.byteSize() < 2u * 1024 * 1024 + 512 * 1024);

    std::vector<PeakPair> view(1920);
    const double perColumn = static_cast<double>(frames) / 1920.0;
    peaks.columns(track, 0, 0.0, perColumn, view);
    std::size_t loud = 0;
    for (const PeakPair& column : view)
        loud += column.max > 0.9f ? 1 : 0;
    CHECK(loud == 3);
    for (const std::size_t i : {std::size_t{0}, std::size_t{87}, std::size_t{1919}}) {
        const auto start = static_cast<std::size_t>(std::floor(i * perColumn));
        const auto end = static_cast<std::size_t>(std::floor((i + 1) * perColumn));
        CHECK(view[i] == bruteForce(track, 0, {start, end - start}));
    }
    std::vector<PeakPair> close(800);
    peaks.columns(track, 0, 1234000.0, 3.0, close);
    CHECK(close[189] == bruteForce(track, 0, {1234567, 3}));
    CHECK(close[189].max > 0.9f);
    CHECK(close[188].max < 0.1f);
}
