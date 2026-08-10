#pragma once

#include "core/CoreExport.h"
#include "formats/audio/SampleBuffer.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace OFS {

// Preview sound output on top of miniaudio. The audio thread holds a reference to the buffer while
// it plays, so edit a copy instead. Without a device play() returns false.
class OFS_CORE_EXPORT AudioEngine
{
public:
    enum class Backend
    {
        System,
        Null, // silent device for tests
    };

    enum class State
    {
        Stopped,
        Playing,
        Paused,
    };

    enum class Event
    {
        Finished,
        DeviceLost,
    };

    explicit AudioEngine(Backend backend = Backend::System);
    ~AudioEngine();
    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    bool isAvailable() const noexcept;
    std::string errorString() const;
    std::string backendName() const;
    std::string deviceName() const;

    bool play(std::shared_ptr<const Formats::SampleBuffer> buffer, std::size_t startFrame = 0,
              std::optional<Formats::SampleRange> loop = std::nullopt);
    void pause();
    bool resume();
    void stop();

    State state() const noexcept;
    // Frame last handed to the device, a bit ahead of what is heard. When stopped, the start frame.
    std::size_t position() const noexcept;
    bool isLooping() const noexcept;

    void setVolume(float volume) noexcept;
    float volume() const noexcept;

    // Runs on a miniaudio thread. It must be quick and must not call the engine.
    void setEventCallback(std::function<void(Event)> callback);

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};

} // namespace OFS
