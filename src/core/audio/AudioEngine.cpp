#include "core/audio/AudioEngine.h"

#include <miniaudio.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <iterator>
#include <mutex>
#include <vector>

namespace OFS {

using Formats::SampleBuffer;
using Formats::SampleRange;
using Formats::SampleType;

namespace {

std::string describe(const char* what, ma_result result)
{
    return std::string(what) + ": " + ma_result_description(result);
}

} // namespace

struct AudioEngine::Impl
{
    explicit Impl(Backend backend);
    ~Impl();

    bool openDevice(std::uint16_t channels, std::uint32_t sampleRate);
    void closeDevice();
    // Set `state` first so the audio thread sees why.
    void stopDevice();
    bool deviceRunning() const noexcept { return deviceReady && ma_device_is_started(&device) != MA_FALSE; }
    void render(float* out, std::size_t frameCount);
    void notify(Event event);

    static void dataCallback(ma_device* device, void* output, const void* input, ma_uint32 frameCount);
    static void notificationCallback(const ma_device_notification* notification);

    ma_context context{};
    ma_device device{};
    bool contextReady = false;
    bool deviceReady = false;
    std::uint16_t deviceChannels = 0;
    std::uint32_t deviceRate = 0;
    std::string error;

    // Written only while the device is stopped, read by the audio thread while it runs.
    std::shared_ptr<const SampleBuffer> buffer;
    std::size_t startFrame = 0;
    bool looping = false;
    SampleRange loop;
    std::size_t drainFrames = 0;

    std::atomic<State> state{State::Stopped};
    std::atomic<std::size_t> position{0};
    std::atomic<float> volume{1.0f};
    std::atomic<bool> ended{false};
    std::atomic<std::size_t> tailFrames{0};
    std::atomic<bool> stopRequested{false};
    std::atomic<bool> deviceLost{false};

    std::mutex callbackMutex;
    std::function<void(Event)> callback;
};

AudioEngine::Impl::Impl(Backend backend)
{
    std::vector<ma_backend> backends;
    if (backend == Backend::Null) {
        backends.push_back(ma_backend_null);
    } else {
        // Every API compiled in except the null device, which would make a missing sound card
        // look like a working one.
        ma_backend enabled[ma_backend_null + 1];
        size_t count = 0;
        if (ma_get_enabled_backends(enabled, std::size(enabled), &count) == MA_SUCCESS) {
            for (size_t i = 0; i < count; ++i) {
                if (enabled[i] != ma_backend_null && enabled[i] != ma_backend_custom)
                    backends.push_back(enabled[i]);
            }
        }
    }
    if (backends.empty()) {
        error = "no audio output API is available";
        return;
    }
    const ma_context_config config = ma_context_config_init();
    const ma_result result =
        ma_context_init(backends.data(), static_cast<ma_uint32>(backends.size()), &config, &context);
    if (result != MA_SUCCESS) {
        error = describe("cannot initialise audio output", result);
        return;
    }
    contextReady = true;
}

AudioEngine::Impl::~Impl()
{
    closeDevice();
    if (contextReady)
        ma_context_uninit(&context);
}

bool AudioEngine::Impl::openDevice(std::uint16_t channels, std::uint32_t sampleRate)
{
    if (deviceReady && deviceChannels == channels && deviceRate == sampleRate)
        return true;
    closeDevice();
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;
    config.playback.channels = channels;
    config.sampleRate = sampleRate;
    config.dataCallback = &Impl::dataCallback;
    config.notificationCallback = &Impl::notificationCallback;
    config.pUserData = this;
    const ma_result result = ma_device_init(&context, &config, &device);
    if (result != MA_SUCCESS) {
        error = describe("cannot open the audio device", result);
        return false;
    }
    deviceReady = true;
    deviceChannels = channels;
    deviceRate = sampleRate;
    // The device buffer (all periods), plus the period miniaudio keeps for fixed-size callbacks
    // and one more as a margin, converted from the device's rate to the buffer's.
    const std::uint64_t internalFrames =
        std::uint64_t{device.playback.internalPeriodSizeInFrames} * (device.playback.internalPeriods + 2);
    const std::uint32_t internalRate =
        device.playback.internalSampleRate != 0 ? device.playback.internalSampleRate : sampleRate;
    drainFrames = static_cast<std::size_t>(internalFrames * sampleRate / internalRate);
    return true;
}

void AudioEngine::Impl::closeDevice()
{
    if (!deviceReady)
        return;
    stopRequested.store(true);
    ma_device_uninit(&device);
    stopRequested.store(false);
    deviceReady = false;
}

void AudioEngine::Impl::stopDevice()
{
    if (!deviceRunning())
        return;
    stopRequested.store(true);
    ma_device_stop(&device);
    stopRequested.store(false);
}

void AudioEngine::Impl::render(float* out, std::size_t frameCount)
{
    // The output starts silent (miniaudio clears it), so returning early plays silence.
    if (state.load(std::memory_order_acquire) != State::Playing)
        return;
    std::size_t done = 0;
    if (!ended.load(std::memory_order_relaxed)) {
        const SampleBuffer& samples = *buffer;
        const std::size_t channels = samples.channels();
        const std::size_t frames = samples.frameCount();
        const float gain = volume.load(std::memory_order_relaxed);
        std::size_t pos = position.load(std::memory_order_relaxed);
        while (done < frameCount) {
            const std::size_t end = looping ? loop.end() : frames;
            if (pos >= end) {
                if (looping) {
                    pos = loop.start;
                    continue;
                }
                ended.store(true, std::memory_order_relaxed);
                break;
            }
            const std::size_t count = std::min(frameCount - done, end - pos);
            float* dst = out + done * channels;
            if (samples.type() == SampleType::Float32) {
                const float* src = samples.floatSamples().data() + pos * channels;
                for (std::size_t i = 0; i < count * channels; ++i)
                    dst[i] = src[i] * gain;
            } else {
                const std::int16_t* src = samples.int16Samples().data() + pos * channels;
                const float scale = gain / 32768.0f;
                for (std::size_t i = 0; i < count * channels; ++i)
                    dst[i] = static_cast<float>(src[i]) * scale;
            }
            pos += count;
            done += count;
        }
        position.store(pos, std::memory_order_relaxed);
    }
    if (ended.load(std::memory_order_relaxed)) {
        // Feed silence until the last samples leave the device buffer, so the end is never cut off.
        const std::size_t tail = tailFrames.load(std::memory_order_relaxed) + (frameCount - done);
        tailFrames.store(tail, std::memory_order_relaxed);
        if (tail >= drainFrames) {
            position.store(startFrame, std::memory_order_relaxed);
            state.store(State::Stopped, std::memory_order_release);
            notify(Event::Finished);
        }
    }
}

void AudioEngine::Impl::notify(Event event)
{
    const std::lock_guard<std::mutex> lock(callbackMutex);
    if (callback)
        callback(event);
}

void AudioEngine::Impl::dataCallback(ma_device* device, void* output, const void*, ma_uint32 frameCount)
{
    static_cast<Impl*>(device->pUserData)->render(static_cast<float*>(output), frameCount);
}

void AudioEngine::Impl::notificationCallback(const ma_device_notification* notification)
{
    if (notification->type != ma_device_notification_type_stopped)
        return;
    auto* self = static_cast<Impl*>(notification->pDevice->pUserData);
    if (self->stopRequested.load())
        return;
    // Stopped although nobody asked: the device went away.
    self->deviceLost.store(true);
    State expected = State::Playing;
    if (self->state.compare_exchange_strong(expected, State::Stopped)) {
        self->position.store(self->startFrame);
        self->notify(Event::DeviceLost);
    }
}

AudioEngine::AudioEngine(Backend backend)
    : d(std::make_unique<Impl>(backend))
{}

AudioEngine::~AudioEngine() = default;

bool AudioEngine::isAvailable() const noexcept
{
    return d->contextReady;
}

std::string AudioEngine::errorString() const
{
    return d->error;
}

std::string AudioEngine::backendName() const
{
    return d->contextReady ? std::string(ma_get_backend_name(d->context.backend)) : std::string();
}

std::string AudioEngine::deviceName() const
{
    if (!d->deviceReady)
        return {};
    char name[MA_MAX_DEVICE_NAME_LENGTH + 1] = {};
    if (ma_device_get_name(&d->device, ma_device_type_playback, name, sizeof(name), nullptr) != MA_SUCCESS)
        return {};
    return name;
}

bool AudioEngine::play(std::shared_ptr<const Formats::SampleBuffer> buffer, std::size_t startFrame,
                       std::optional<Formats::SampleRange> loop)
{
    d->state.store(State::Stopped);
    d->stopDevice();
    if (!d->deviceRunning())
        d->buffer.reset();
    if (!d->contextReady)
        return false;
    if (!buffer || buffer->empty()) {
        d->error = "there is nothing to play";
        return false;
    }
    if (buffer->sampleRate() == 0) {
        d->error = "the sound has a sample rate of 0";
        return false;
    }
    if (d->deviceLost.exchange(false))
        d->closeDevice();
    if (!d->openDevice(buffer->channels(), buffer->sampleRate()))
        return false;

    const std::size_t frames = buffer->frameCount();
    d->buffer = std::move(buffer);
    d->startFrame = startFrame < frames ? startFrame : 0;
    d->loop = loop ? loop->clipped(frames) : SampleRange{};
    d->looping = !d->loop.empty();
    d->position.store(d->startFrame);
    d->ended.store(false);
    d->tailFrames.store(0);
    d->state.store(State::Playing, std::memory_order_release);
    const ma_result result = ma_device_start(&d->device);
    if (result != MA_SUCCESS) {
        d->state.store(State::Stopped);
        d->buffer.reset();
        d->error = describe("cannot start the audio device", result);
        return false;
    }
    d->error.clear();
    return true;
}

void AudioEngine::pause()
{
    if (d->state.load() != State::Playing)
        return;
    d->state.store(State::Paused);
    d->stopDevice();
}

bool AudioEngine::resume()
{
    const State current = d->state.load();
    if (current != State::Paused)
        return current == State::Playing;
    d->state.store(State::Playing, std::memory_order_release);
    const ma_result result = ma_device_start(&d->device);
    if (result != MA_SUCCESS) {
        d->state.store(State::Stopped);
        d->position.store(d->startFrame);
        d->buffer.reset();
        d->error = describe("cannot restart the audio device", result);
        return false;
    }
    d->error.clear();
    return true;
}

void AudioEngine::stop()
{
    d->state.store(State::Stopped);
    d->stopDevice();
    d->position.store(d->startFrame);
    // Only drop the samples once the audio thread is surely done with them.
    if (!d->deviceRunning())
        d->buffer.reset();
}

AudioEngine::State AudioEngine::state() const noexcept
{
    return d->state.load();
}

std::size_t AudioEngine::position() const noexcept
{
    return d->position.load(std::memory_order_relaxed);
}

bool AudioEngine::isLooping() const noexcept
{
    return d->looping && d->state.load() != State::Stopped;
}

void AudioEngine::setVolume(float volume) noexcept
{
    d->volume.store(std::isnan(volume) ? 0.0f : std::clamp(volume, 0.0f, 1.0f));
}

float AudioEngine::volume() const noexcept
{
    return d->volume.load();
}

void AudioEngine::setEventCallback(std::function<void(Event)> callback)
{
    const std::lock_guard<std::mutex> lock(d->callbackMutex);
    d->callback = std::move(callback);
}

} // namespace OFS
