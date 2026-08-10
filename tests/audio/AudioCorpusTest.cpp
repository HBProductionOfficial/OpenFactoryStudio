#include "formats/TestEnv.h"
#include "formats/audio/AudioCodec.h"
#include "formats/audio/Resampler.h"
#include "formats/audio/WaveformPeaks.h"
#include "formats/snd/SndDocument.h"
#include "formats/util/FileIo.h"
#include "formats/wav/WavFile.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cctype>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

using namespace OFS::Formats;
using namespace OFS::Formats::Test;

namespace {

std::string lowerExtension(const std::filesystem::path& path)
{
    std::string ext = path.extension().string();
    for (char& c : ext)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return ext;
}

std::vector<std::filesystem::path> filesWithExtensions(const std::filesystem::path& root,
                                                       const std::vector<std::string>& extensions)
{
    std::vector<std::filesystem::path> files;
    std::error_code ec;
    for (auto it = std::filesystem::recursive_directory_iterator(
             root, std::filesystem::directory_options::skip_permission_denied, ec);
         !ec && it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
        if (it->is_regular_file(ec) &&
            std::find(extensions.begin(), extensions.end(), lowerExtension(it->path())) != extensions.end())
            files.push_back(it->path());
    }
    std::sort(files.begin(), files.end());
    return files;
}

std::string display(const std::filesystem::path& path)
{
    std::string text;
    for (const char8_t c : path.u8string())
        text.push_back(static_cast<char>(c));
    return text;
}

struct SoundStats
{
    std::size_t sounds = 0;
    std::size_t decoded = 0;
    std::size_t matchesWav = 0;
    std::size_t wavRoundTrips = 0;
    std::size_t flacChecked = 0;
    std::size_t flacRoundTrips = 0;
    std::map<std::string, std::size_t> layouts;
    std::vector<std::string> problems;
};

// Only every `flacEvery`-th sound is also re-encoded as FLAC, because that is slow in debug builds.
void checkSound(const SndSound& sound, const std::string& where, SoundStats& st, std::size_t flacEvery = 1)
{
    ++st.sounds;
    const std::string name = where + " sound " + std::to_string(sound.group) + "," + std::to_string(sound.number);
    auto audio = decodeAudio(sound.data);
    if (!audio) {
        st.problems.push_back(name + ": " + audio.error().message);
        return;
    }
    ++st.decoded;
    const SampleBuffer& buffer = audio->buffer;
    st.layouts[std::to_string(audio->bitsPerSample) + "-bit " + std::to_string(buffer.channels()) + "ch"]++;

    auto wav = WavFile::parse(sound.data);
    if (wav && wav->frameCount() == buffer.frameCount() && wav->format.channels == buffer.channels() &&
        wav->format.sampleRate == buffer.sampleRate()) {
        bool same = true;
        for (std::size_t f = 0; f < buffer.frameCount() && same; f += 1 + buffer.frameCount() / 64) {
            for (std::uint16_t c = 0; c < buffer.channels(); ++c)
                same = same && static_cast<float>(wav->sample(f, c)) == buffer.sample(f, c);
        }
        if (same)
            ++st.matchesWav;
        else
            st.problems.push_back(name + ": samples differ from WavFile::sample()");
    } else {
        st.problems.push_back(name + ": layout differs from the WAV reader");
    }

    EncodeOptions options;
    options.bitsPerSample = audio->bitsPerSample;
    auto reencoded = encodeAudio(buffer, options);
    if (reencoded && decodeAudio(*reencoded).valueOr({}).buffer == buffer)
        ++st.wavRoundTrips;
    else
        st.problems.push_back(name + ": WAV re-encoding is not lossless");
    if (st.decoded % flacEvery != 0)
        return;
    ++st.flacChecked;
    options.format = AudioFileFormat::Flac;
    auto flac = encodeAudio(buffer, options);
    if (flac && decodeAudio(*flac).valueOr({}).buffer == buffer)
        ++st.flacRoundTrips;
    else
        st.problems.push_back(name + ": FLAC re-encoding is not lossless");
}

std::string summary(const std::string& title, const SoundStats& st)
{
    std::ostringstream out;
    out << title << ": " << st.sounds << " sounds, decoded " << st.decoded << ", equal to the WAV reader "
        << st.matchesWav << ", lossless WAV re-encodes " << st.wavRoundTrips << ", lossless FLAC re-encodes "
        << st.flacRoundTrips << "/" << st.flacChecked;
    for (const auto& [layout, n] : st.layouts)
        out << "; " << layout << " " << n;
    for (std::size_t i = 0; i < st.problems.size() && i < 20; ++i)
        out << "\n  " << st.problems[i];
    return out.str();
}

void requireAllGood(const SoundStats& st)
{
    CHECK(st.sounds > 0);
    CHECK(st.decoded == st.sounds);
    CHECK(st.matchesWav == st.sounds);
    CHECK(st.wavRoundTrips == st.sounds);
    CHECK(st.flacChecked > 0);
    CHECK(st.flacRoundTrips == st.flacChecked);
}

} // namespace

TEST_CASE("KFM: every sound decodes, re-encodes losslessly and resamples", "[corpus][kfm]")
{
    const auto dir = corpusDir("OFS_CORPUS_KFM");
    if (!dir)
        SKIP("OFS_CORPUS_KFM is not set");
    auto doc = SndDocument::load(*dir / "kfm.snd");
    REQUIRE(doc.ok());
    SoundStats st;
    for (const SndSound& sound : doc->sounds) {
        checkSound(sound, "kfm.snd", st);
        auto audio = decodeAudio(sound.data);
        REQUIRE(audio.ok());
        EncodeOptions vorbis;
        vorbis.format = AudioFileFormat::OggVorbis;
        auto ogg = encodeAudio(audio->buffer, vorbis);
        REQUIRE(ogg.ok());
        auto back = decodeAudio(*ogg);
        REQUIRE(back.ok());
        CHECK(back->buffer.frameCount() == audio->buffer.frameCount());
        auto resampled = resample(audio->buffer, 44100);
        REQUIRE(resampled.ok());
        WaveformPeaks peaks;
        peaks.build(audio->buffer);
        CHECK(peaks.isComplete(audio->buffer));
    }
    const std::string text = summary("KFM SND", st);
    std::cout << text << std::endl;
    WARN(text);
    requireAllGood(st);
}

TEST_CASE("MUGEN: every sound of every SND file decodes and re-encodes losslessly", "[corpus][mugen]")
{
    const auto dir = corpusDir("OFS_CORPUS_MUGEN");
    if (!dir)
        SKIP("OFS_CORPUS_MUGEN is not set");
    SoundStats st;
    std::size_t files = 0;
    for (const auto& file : filesWithExtensions(*dir, {".snd"})) {
        auto doc = SndDocument::load(file);
        if (!doc)
            continue;
        ++files;
        for (const SndSound& sound : doc->sounds)
            checkSound(sound, display(file), st, 8);
    }
    const std::string text = summary(std::to_string(files) + " SND files", st);
    std::cout << text << std::endl;
    WARN(text);
    CHECK(files > 0);
    requireAllGood(st);
}

TEST_CASE("MUGEN: loose audio files decode by their content", "[corpus][mugen]")
{
    const auto dir = corpusDir("OFS_CORPUS_MUGEN");
    if (!dir)
        SKIP("OFS_CORPUS_MUGEN is not set");
    std::map<std::string, std::size_t> formats;
    std::size_t files = 0;
    std::size_t decodedFiles = 0;
    std::vector<std::string> problems;
    for (const auto& file : filesWithExtensions(*dir, {".wav", ".ogg", ".mp3", ".flac"})) {
        ++files;
        auto bytes = readFile(file);
        REQUIRE(bytes.ok());
        auto audio = decodeAudio(*bytes);
        if (!audio) {
            problems.push_back(display(file) + ": " + audio.error().message);
            continue;
        }
        ++decodedFiles;
        formats[std::string(audioFormatName(audio->format)) + " " + audio->codec]++;
        if (audio->buffer.empty())
            problems.push_back(display(file) + ": no samples");
    }
    std::ostringstream report;
    report << files << " audio files, decoded " << decodedFiles;
    for (const auto& [format, n] : formats)
        report << "; " << format << " " << n;
    for (std::size_t i = 0; i < problems.size() && i < 20; ++i)
        report << "\n  " << problems[i];
    std::cout << report.str() << std::endl;
    WARN(report.str());
    CHECK(decodedFiles == files);
}
