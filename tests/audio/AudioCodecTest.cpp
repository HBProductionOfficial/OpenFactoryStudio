#include "formats/audio/AudioCodec.h"
#include "audio/AudioTestUtil.h"
#include "formats/util/ByteWriter.h"

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <filesystem>
#include <random>
#include <string>
#include <string_view>

using namespace OFS::Formats;
using namespace OFS::Formats::Test;

namespace {

Bytes encoded(const SampleBuffer& buffer, const EncodeOptions& options)
{
    auto bytes = encodeAudio(buffer, options);
    REQUIRE(bytes.ok());
    return std::move(*bytes);
}

DecodedAudio decoded(ByteSpan data, const DecodeOptions& options = {})
{
    auto audio = decodeAudio(data, options);
    if (!audio)
        FAIL(audio.error().message);
    return std::move(*audio);
}

Bytes bytesOf(std::initializer_list<int> values)
{
    Bytes out;
    for (const int v : values)
        out.push_back(static_cast<std::byte>(v));
    return out;
}

// Silent MPEG-1 layer III frames: 44.1 kHz, 128 kbit/s, mono, no CRC, all side information zero.
Bytes silentMp3Frames(std::size_t count)
{
    constexpr std::size_t kFrameBytes = 144 * 128000 / 44100; // 417, no padding
    Bytes out;
    for (std::size_t i = 0; i < count; ++i) {
        const Bytes header = bytesOf({0xFF, 0xFB, 0x90, 0xC0});
        out.insert(out.end(), header.begin(), header.end());
        out.resize(out.size() + kFrameBytes - header.size());
    }
    return out;
}

WavFile compressedWav(std::uint16_t tag, std::uint16_t channels, std::uint16_t bits, std::uint16_t blockAlign,
                      Bytes samples, Bytes extension = {})
{
    WavFile wav;
    wav.format.formatTag = tag;
    wav.format.channels = channels;
    wav.format.sampleRate = 8000;
    wav.format.bitsPerSample = bits;
    wav.format.blockAlign = blockAlign;
    wav.format.byteRate = 8000 * blockAlign;
    wav.format.extension = std::move(extension);
    wav.samples = std::move(samples);
    return wav;
}

std::vector<std::int16_t> decodedInt16(const WavFile& wav)
{
    const Bytes file = wav.write();
    const DecodedAudio audio = decoded(file);
    REQUIRE(audio.buffer.type() == SampleType::Int16);
    return int16Values(audio.buffer);
}

} // namespace

TEST_CASE("Audio formats are recognised from their content", "[audio][codec]")
{
    const SampleBuffer tone = sine(SampleType::Int16, 1, 22050, 2000, 440.0);
    const Bytes wav = encoded(tone, {AudioFileFormat::Wav});
    const Bytes flac = encoded(tone, {AudioFileFormat::Flac});
    EncodeOptions vorbisOptions;
    vorbisOptions.format = AudioFileFormat::OggVorbis;
    const Bytes ogg = encoded(tone, vorbisOptions);

    CHECK(detectAudioFormat(wav) == AudioFileFormat::Wav);
    CHECK(detectAudioFormat(flac) == AudioFileFormat::Flac);
    CHECK(detectAudioFormat(ogg) == AudioFileFormat::OggVorbis);
    CHECK(detectAudioFormat(silentMp3Frames(3)) == AudioFileFormat::Mp3);
    CHECK(detectAudioFormat(silentMp3Frames(1)) == AudioFileFormat::Mp3);

    Bytes tagged = bytesOf({'I', 'D', '3', 4, 0, 0, 0, 0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0});
    Bytes taggedFlac = tagged;
    taggedFlac.insert(taggedFlac.end(), flac.begin(), flac.end());
    CHECK(detectAudioFormat(taggedFlac) == AudioFileFormat::Flac);
    Bytes taggedMp3 = tagged;
    const Bytes frames = silentMp3Frames(2);
    taggedMp3.insert(taggedMp3.end(), frames.begin(), frames.end());
    CHECK(detectAudioFormat(taggedMp3) == AudioFileFormat::Mp3);

    Bytes oggFlac = bytesOf({'O', 'g', 'g', 'S', 0, 2, 0, 0, 0, 0, 0,  0,    0,   0,   1,   0,  0,
                             0,   0,   0,   0,   0, 0, 0, 0, 0, 1, 51, 0x7F, 'F', 'L', 'A', 'C'});
    CHECK(detectAudioFormat(oggFlac) == AudioFileFormat::Flac);
    Bytes opus = oggFlac;
    opus.resize(40);
    std::memcpy(opus.data() + 28, "OpusHead", 8);
    CHECK(detectAudioFormat(opus) == AudioFileFormat::Unknown);
    auto opusError = decodeAudio(opus);
    REQUIRE_FALSE(opusError.ok());
    CHECK(opusError.error().message.find("Ogg") != std::string::npos);

    CHECK(detectAudioFormat({}) == AudioFileFormat::Unknown);
    CHECK(detectAudioFormat(toBytes(std::string_view("RIFF\x10\0\0\0AVI LIST", 16))) == AudioFileFormat::Unknown);
    CHECK(detectAudioFormat(toBytes("hello, world")) == AudioFileFormat::Unknown);
    CHECK(detectAudioFormat(bytesOf({0xFF, 0xFB, 0xF0, 0xC0, 0, 0})) == AudioFileFormat::Unknown);
    Bytes falseSync = silentMp3Frames(1);
    falseSync.push_back(std::byte{0x12});
    falseSync.resize(falseSync.size() + 8);
    CHECK(detectAudioFormat(falseSync) == AudioFileFormat::Unknown);
    CHECK_FALSE(decodeAudio(toBytes("hello, world")).ok());

    CHECK(audioFormatName(AudioFileFormat::OggVorbis) == "Ogg Vorbis");
    CHECK(audioFormatExtension(AudioFileFormat::Flac) == "flac");
    CHECK(audioFormatExtension(AudioFileFormat::Unknown).empty());
}

TEST_CASE("WAV files decode to the samples they were written from", "[audio][codec][wav]")
{
    const SampleBuffer ints = noise(SampleType::Int16, 2, 22050, 1000, 7);
    const SampleBuffer floats = *quantize(noise(SampleType::Float32, 1, 48000, 1000, 8), 24);

    struct Case
    {
        const SampleBuffer* buffer;
        std::uint16_t bits;
        bool floatingPoint;
        SampleType type;
        const char* codec;
    };
    for (const Case& c :
         {Case{&ints, 16, false, SampleType::Int16, "PCM"}, Case{&floats, 24, false, SampleType::Float32, "PCM"},
          Case{&floats, 32, false, SampleType::Float32, "PCM"},
          Case{&floats, 32, true, SampleType::Float32, "IEEE float"},
          Case{&floats, 64, true, SampleType::Float32, "IEEE float"}}) {
        EncodeOptions options;
        options.bitsPerSample = c.bits;
        options.floatingPoint = c.floatingPoint;
        const DecodedAudio audio = decoded(encoded(*c.buffer, options));
        CHECK(audio.format == AudioFileFormat::Wav);
        CHECK(audio.bitsPerSample == c.bits);
        CHECK(audio.codec == c.codec);
        CHECK(audio.buffer.type() == c.type);
        CHECK(audio.buffer == *c.buffer);
    }

    EncodeOptions eightBit;
    eightBit.bitsPerSample = 8;
    const SampleBuffer coarse = *quantize(ints, 8);
    const DecodedAudio audio = decoded(encoded(ints, eightBit));
    CHECK(audio.bitsPerSample == 8);
    CHECK(audio.buffer == coarse);

    DecodeOptions asFloat;
    asFloat.sampleType = SampleType::Float32;
    CHECK(decoded(encoded(ints, {}), asFloat).buffer == convertSampleType(ints, SampleType::Float32));
}

TEST_CASE("FLAC encoding is lossless", "[audio][codec][flac]")
{
    SECTION("16-bit stereo")
    {
        const SampleBuffer buffer = noise(SampleType::Int16, 2, 44100, 10000, 3);
        EncodeOptions options;
        options.format = AudioFileFormat::Flac;
        const Bytes flac = encoded(buffer, options);
        CHECK(flac.size() > 42);
        CHECK(toText(ByteSpan(flac).first(4)) == "fLaC");
        const DecodedAudio audio = decoded(flac);
        CHECK(audio.format == AudioFileFormat::Flac);
        CHECK(audio.codec == "FLAC");
        CHECK(audio.bitsPerSample == 16);
        CHECK(audio.buffer == buffer);
        CHECK(encoded(buffer, options) == flac);
        options.flacCompressionLevel = 0;
        CHECK(decoded(encoded(buffer, options)).buffer == buffer);
        options.flacCompressionLevel = 8;
        CHECK(decoded(encoded(buffer, options)).buffer == buffer);
    }
    SECTION("other depths and layouts")
    {
        const SampleBuffer deep = *quantize(sine(SampleType::Float32, 2, 96000, 5000, 1000.0, 0.9), 24);
        EncodeOptions options;
        options.format = AudioFileFormat::Flac;
        options.bitsPerSample = 24;
        const DecodedAudio audio = decoded(encoded(deep, options));
        CHECK(audio.bitsPerSample == 24);
        CHECK(audio.buffer.type() == SampleType::Float32);
        CHECK(audio.buffer == deep);
        EncodeOptions eight;
        eight.format = AudioFileFormat::Flac;
        eight.bitsPerSample = 8;
        const SampleBuffer coarse = *quantize(noise(SampleType::Int16, 1, 8000, 3000, 4), 8);
        CHECK(decoded(encoded(coarse, eight)).buffer == coarse);

        EncodeOptions sixChannels;
        sixChannels.format = AudioFileFormat::Flac;
        const SampleBuffer surround = noise(SampleType::Int16, 6, 48000, 2000, 5);
        CHECK(decoded(encoded(surround, sixChannels)).buffer == surround);

        Bytes tagged = bytesOf({'I', 'D', '3', 3, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0});
        const Bytes flac = encoded(coarse, eight);
        tagged.insert(tagged.end(), flac.begin(), flac.end());
        CHECK(decoded(tagged).buffer == coarse);
    }
    SECTION("limits")
    {
        EncodeOptions options;
        options.format = AudioFileFormat::Flac;
        options.bitsPerSample = 12;
        CHECK_FALSE(encodeAudio(int16Buffer({1, 2}), options).ok());
        options.bitsPerSample = 32;
        CHECK_FALSE(encodeAudio(int16Buffer({1, 2}), options).ok());
        options.bitsPerSample = 16;
        CHECK_FALSE(encodeAudio(SampleBuffer(SampleType::Int16, 9, 8000, 10), options).ok());
        const DecodedAudio empty = decoded(encoded(SampleBuffer(SampleType::Int16, 1, 8000), options));
        CHECK(empty.buffer.empty());
        Bytes truncated = encoded(noise(SampleType::Int16, 1, 8000, 4000, 6), options);
        truncated.resize(30);
        CHECK_FALSE(decodeAudio(truncated).ok());
    }
}

TEST_CASE("Ogg Vorbis encoding keeps the sound", "[audio][codec][vorbis]")
{
    SECTION("44.1 kHz stereo")
    {
        const SampleBuffer tone = sine(SampleType::Float32, 2, 44100, 44100, 441.0, 0.5);
        EncodeOptions options;
        options.format = AudioFileFormat::OggVorbis;
        options.vorbisQuality = 0.5f;
        const Bytes ogg = encoded(tone, options);
        CHECK(toText(ByteSpan(ogg).first(4)) == "OggS");
        CHECK(ogg.size() < tone.byteSize() / 4);
        CHECK(encoded(tone, options) == ogg);

        const DecodedAudio audio = decoded(ogg);
        CHECK(audio.format == AudioFileFormat::OggVorbis);
        CHECK(audio.codec == "Vorbis");
        CHECK(audio.bitsPerSample == 0);
        CHECK(audio.buffer.type() == SampleType::Float32);
        CHECK(audio.buffer.channels() == 2);
        CHECK(audio.buffer.sampleRate() == 44100);
        CHECK(audio.buffer.frameCount() == tone.frameCount());
        CHECK(std::fabs(toneLevel(audio.buffer, 0, 441.0, 4410, 35280) - 0.5) < 0.02);
        CHECK(std::fabs(toneLevel(audio.buffer, 1, 882.0, 4410, 35280) - 0.5) < 0.02);
        CHECK(maxDifference(audio.buffer, tone, 4410, 35280) < 0.05);
    }
    SECTION("game sound rates, mono")
    {
        for (const std::uint32_t rate : {11025u, 17700u, 22050u, 48000u}) {
            const SampleBuffer tone = sine(SampleType::Int16, 1, rate, rate / 2, 300.0, 0.7);
            EncodeOptions options;
            options.format = AudioFileFormat::OggVorbis;
            const DecodedAudio audio = decoded(encoded(tone, options));
            CHECK(audio.buffer.sampleRate() == rate);
            CHECK(audio.buffer.frameCount() == tone.frameCount());
            CHECK(std::fabs(toneLevel(audio.buffer, 0, 300.0, rate / 10, rate / 4) - 0.7) < 0.05);
            DecodeOptions asInt;
            asInt.sampleType = SampleType::Int16;
            CHECK(decoded(encoded(tone, options), asInt).buffer.type() == SampleType::Int16);
        }
    }
    SECTION("empty and broken streams")
    {
        EncodeOptions options;
        options.format = AudioFileFormat::OggVorbis;
        const DecodedAudio empty = decoded(encoded(SampleBuffer(SampleType::Float32, 2, 22050), options));
        CHECK(empty.buffer.empty());
        CHECK(empty.buffer.channels() == 2);

        Bytes truncated = encoded(sine(SampleType::Float32, 1, 22050, 22050, 500.0), options);
        truncated.resize(truncated.size() / 2);
        auto partial = decodeAudio(truncated);
        if (partial.ok())
            CHECK(partial->buffer.frameCount() < 22050);
        truncated.resize(60);
        CHECK_FALSE(decodeAudio(truncated).ok());
    }
}

TEST_CASE("MP3 frames decode with dr_mp3", "[audio][codec][mp3]")
{
    const Bytes mp3 = silentMp3Frames(20);
    const DecodedAudio audio = decoded(mp3);
    CHECK(audio.format == AudioFileFormat::Mp3);
    CHECK(audio.codec == "MPEG audio");
    CHECK(audio.bitsPerSample == 0);
    CHECK(audio.buffer.type() == SampleType::Int16);
    CHECK(audio.buffer.channels() == 1);
    CHECK(audio.buffer.sampleRate() == 44100);
    CHECK(audio.buffer.frameCount() % 1152 == 0);
    CHECK(audio.buffer.frameCount() >= 1152 * 18);
    CHECK(rms(audio.buffer, 0, 0, audio.buffer.frameCount()) == 0.0);

    // MP3 inside a WAV file is format tag 0x55.
    WavFile wav = compressedWav(0x55, 1, 0, 1, mp3);
    wav.format.sampleRate = 44100;
    const DecodedAudio inWav = decoded(wav.write());
    CHECK(inWav.format == AudioFileFormat::Wav);
    CHECK(inWav.codec == "MPEG Layer III");
    CHECK(inWav.buffer == audio.buffer);

    Bytes truncated = mp3;
    truncated.resize(600);
    auto partial = decodeAudio(truncated);
    if (partial.ok())
        CHECK(partial->buffer.frameCount() <= 1152 * 2);
    CHECK_FALSE(canEncode(AudioFileFormat::Mp3));
    EncodeOptions options;
    options.format = AudioFileFormat::Mp3;
    CHECK_FALSE(encodeAudio(audio.buffer, options).ok());
}

TEST_CASE("A-law and mu-law WAV files expand to 16 bits", "[audio][codec][wav]")
{
    const WavFile alaw = compressedWav(6, 1, 8, 1, bytesOf({0xD5, 0x55, 0xAA, 0x2A, 0xD4}));
    CHECK(decodedInt16(alaw) == std::vector<std::int16_t>{8, -8, 32256, -32256, 24});
    const WavFile mulaw = compressedWav(7, 1, 8, 1, bytesOf({0xFF, 0x7F, 0x80, 0x00, 0xFE}));
    CHECK(decodedInt16(mulaw) == std::vector<std::int16_t>{0, 0, 32124, -32124, 8});

    const DecodedAudio audio = decoded(compressedWav(7, 2, 8, 2, bytesOf({0x80, 0x00})).write());
    CHECK(audio.codec == "mu-law");
    CHECK(audio.bitsPerSample == 8);
    CHECK(audio.buffer.channels() == 2);
    CHECK(audio.buffer.frameCount() == 1);
}

TEST_CASE("IMA ADPCM WAV files decode block by block", "[audio][codec][wav][adpcm]")
{
    // Mono, 8-byte blocks: a 4-byte header (first sample, step index) and eight 4-bit codes.
    const Bytes extension = bytesOf({2, 0, 9, 0});                             // cbSize 2, 9 samples per block
    const Bytes blocks = bytesOf({0x00, 0x00, 0, 0, 0x77, 0x77, 0x00, 0x00,    // from 0, index 0
                                  0xE8, 0x03, 10, 0, 0x0F, 0x00, 0x00, 0x00}); // from 1000, index 10
    const WavFile mono = compressedWav(0x11, 1, 4, 8, blocks, extension);
    const std::vector<std::int16_t> expected = {0,    11,  41,  104, 240, 259, 276, 292, 306,
                                                1000, 966, 971, 975, 979, 982, 985, 988, 990};
    CHECK(decodedInt16(mono) == expected);
    const DecodedAudio audio = decoded(mono.write());
    CHECK(audio.codec == "IMA ADPCM");
    CHECK(audio.bitsPerSample == 4);

    WavFile trimmed = mono;
    trimmed.chunks.push_back(WavChunk{{'f', 'a', 'c', 't'}, bytesOf({15, 0, 0, 0})});
    CHECK(decodedInt16(trimmed) == std::vector<std::int16_t>(expected.begin(), expected.begin() + 15));

    WavFile cut = mono;
    cut.samples.resize(14);
    CHECK(decodedInt16(cut) == std::vector<std::int16_t>(expected.begin(), expected.begin() + 10));

    const Bytes stereoBlock = bytesOf({0x00, 0x00, 0, 0, 0x64, 0x00, 0, 0,   // left from 0, right from 100
                                       0x77, 0x77, 0x77, 0x77, 0, 0, 0, 0}); // left codes 7, right codes 0
    const std::vector<std::int16_t> left = {0, 11, 41, 104, 240, 533, 1164, 2521, 5431};
    std::vector<std::int16_t> interleaved;
    for (const std::int16_t l : left) {
        interleaved.push_back(l);
        interleaved.push_back(100);
    }
    CHECK(decodedInt16(compressedWav(0x11, 2, 4, 16, stereoBlock)) == interleaved);

    CHECK_FALSE(decodeAudio(compressedWav(0x11, 1, 4, 3, blocks).write()).ok());
}

TEST_CASE("Microsoft ADPCM WAV files decode block by block", "[audio][codec][wav][adpcm]")
{
    // Mono, 9-byte blocks: predictor index, delta, second and first sample, then four codes.
    const Bytes blocks = bytesOf({0, 16, 0, 100, 0, 50, 0, 0x17, 0x8F, //
                                  1, 16, 0, 100, 0, 50, 0, 0x00, 0x00});
    const std::vector<std::int16_t> expected = {50, 100, 116, 228, -76, -190, 50, 100, 150, 200, 250, 300};
    CHECK(decodedInt16(compressedWav(2, 1, 4, 9, blocks)) == expected);

    ByteWriter extension;
    extension.u16(32); // cbSize
    extension.u16(6);  // samples per block
    extension.u16(7);  // coefficient pairs
    for (const int c : {256, 0, 512, -256, 0, 0, 192, 64, 240, 0, 460, -208, 392, -232})
        extension.s16(static_cast<std::int16_t>(c));
    const WavFile explicitCoefficients = compressedWav(2, 1, 4, 9, blocks, extension.take());
    CHECK(decodedInt16(explicitCoefficients) == expected);
    const DecodedAudio audio = decoded(explicitCoefficients.write());
    CHECK(audio.codec == "Microsoft ADPCM");

    Bytes badPredictor = blocks;
    badPredictor[0] = std::byte{9};
    CHECK_FALSE(decodeAudio(compressedWav(2, 1, 4, 9, badPredictor).write()).ok());
}

TEST_CASE("Unsupported WAV encodings are reported", "[audio][codec][wav]")
{
    auto wma = decodeAudio(compressedWav(0x0161, 2, 16, 4, Bytes(16)).write());
    REQUIRE_FALSE(wma.ok());
    CHECK(wma.error().message.find("0x0161") != std::string::npos);
    CHECK_FALSE(decodeAudio(compressedWav(1, 0, 16, 2, Bytes(4)).write()).ok());
}

TEST_CASE("Audio files load and save through the file system", "[audio][codec]")
{
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / ("ofs-audio-test-" + std::to_string(std::random_device{}()) + ".flac");
    const SampleBuffer buffer = noise(SampleType::Int16, 2, 32000, 2048, 9);
    EncodeOptions options;
    options.format = AudioFileFormat::Flac;
    REQUIRE(saveAudio(buffer, path, options).ok());
    auto loaded = loadAudio(path);
    std::error_code ec;
    std::filesystem::remove(path, ec);
    REQUIRE(loaded.ok());
    CHECK(loaded->format == AudioFileFormat::Flac);
    CHECK(loaded->buffer == buffer);
    CHECK_FALSE(loadAudio(path).ok());
    CHECK(canEncode(AudioFileFormat::Wav));
    CHECK(canEncode(AudioFileFormat::OggVorbis));
    CHECK(canEncode(AudioFileFormat::Flac));
}
