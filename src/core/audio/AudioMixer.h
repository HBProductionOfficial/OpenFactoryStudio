#pragma once

#include "core/CoreExport.h"
#include "core/audio/AudioEngine.h"
#include "formats/audio/SampleBuffer.h"

#include <memory>
#include <string>
#include <vector>

namespace OFS {

// Game sound output. A new sound on a busy player channel replaces the old one.
class OFS_CORE_EXPORT AudioMixer
{
public:
    static constexpr std::uint32_t Rate = 44100;

    // Stereo float frames at Rate.
    struct Sound
    {
        std::vector<float> samples;
        std::size_t frames() const { return samples.size() / 2; }
    };
    static std::shared_ptr<const Sound> prepare(const Formats::SampleBuffer& buffer);

    explicit AudioMixer(AudioEngine::Backend backend = AudioEngine::Backend::System);
    ~AudioMixer();
    AudioMixer(const AudioMixer&) = delete;
    AudioMixer& operator=(const AudioMixer&) = delete;

    bool isAvailable() const;
    std::string errorString() const;

    // Channel -1 takes a free voice. Speed 2 is an octave higher.
    bool play(std::shared_ptr<const Sound> sound, int owner, int channel, float volume = 1, float pan = 0,
              bool loop = false, bool lowPriority = false, float speed = 1);
    void stop(int owner, int channel);
    void stopAll();
    void setPan(int owner, int channel, float pan);
    int voices() const;
    // For tests. With the Null backend nothing else mixes.
    std::vector<float> mix(std::size_t frames);

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};

} // namespace OFS
