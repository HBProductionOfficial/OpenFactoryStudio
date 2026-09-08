#include "core/audio/AudioMixer.h"

#include "formats/audio/Resampler.h"

#include <miniaudio.h>

#include <algorithm>
#include <mutex>

namespace OFS {

namespace {

struct Voice
{
    std::shared_ptr<const AudioMixer::Sound> sound;
    double position = 0;
    double speed = 1;
    int owner = 0;
    int channel = -1;
    float left = 1;
    float right = 1;
    float volume = 1;
    bool loop = false;
};

void setGains(Voice& v, float pan)
{
    pan = std::clamp(pan, -1.0f, 1.0f);
    v.left = v.volume * std::min(1.0f, 1.0f - pan);
    v.right = v.volume * std::min(1.0f, 1.0f + pan);
}

} // namespace

struct AudioMixer::Impl
{
    ma_context context{};
    ma_device device{};
    bool contextReady = false;
    bool deviceReady = false;
    std::string error;
    std::mutex mutex;
    std::vector<Voice> voices;

    void render(float* out, std::size_t frames)
    {
        std::fill(out, out + frames * 2, 0.0f);
        std::lock_guard lock(mutex);
        for (Voice& v : voices) {
            const double length = double(v.sound->frames());
            const std::vector<float>& s = v.sound->samples;
            for (std::size_t f = 0; f < frames; ++f) {
                if (v.position >= length) {
                    if (!v.loop || length == 0)
                        break;
                    v.position -= length;
                }
                const std::size_t at = std::size_t(v.position);
                out[f * 2] += s[at * 2] * v.left;
                out[f * 2 + 1] += s[at * 2 + 1] * v.right;
                v.position += v.speed;
            }
        }
        voices.erase(std::remove_if(voices.begin(), voices.end(),
                                    [](const Voice& v) { return !v.loop && v.position >= double(v.sound->frames()); }),
                     voices.end());
    }

    static void dataCallback(ma_device* device, void* output, const void*, ma_uint32 frameCount)
    {
        static_cast<Impl*>(device->pUserData)->render(static_cast<float*>(output), frameCount);
    }
};

std::shared_ptr<const AudioMixer::Sound> AudioMixer::prepare(const Formats::SampleBuffer& buffer)
{
    auto sound = std::make_shared<Sound>();
    if (buffer.empty() || buffer.channels() == 0)
        return sound;
    Formats::SampleBuffer source = buffer;
    if (source.sampleRate() != Rate) {
        auto resampled = Formats::resample(source, Rate);
        if (resampled)
            source = std::move(resampled.value());
    }
    const std::size_t frames = source.frameCount();
    sound->samples.resize(frames * 2);
    for (std::size_t f = 0; f < frames; ++f) {
        const float left = source.sample(f, 0);
        const float right = source.channels() > 1 ? source.sample(f, 1) : left;
        sound->samples[f * 2] = left;
        sound->samples[f * 2 + 1] = right;
    }
    return sound;
}

AudioMixer::AudioMixer(AudioEngine::Backend backend)
    : d(std::make_unique<Impl>())
{
    std::vector<ma_backend> backends;
    if (backend == AudioEngine::Backend::Null)
        backends.push_back(ma_backend_null);
    ma_backend enabled[MA_BACKEND_COUNT];
    size_t count = 0;
    if (backends.empty() && ma_get_enabled_backends(enabled, MA_BACKEND_COUNT, &count) == MA_SUCCESS) {
        for (size_t i = 0; i < count; ++i) {
            if (enabled[i] != ma_backend_null && enabled[i] != ma_backend_custom)
                backends.push_back(enabled[i]);
        }
    }
    if (backends.empty()) {
        d->error = "no audio output API is available";
        return;
    }
    const ma_context_config contextConfig = ma_context_config_init();
    if (ma_context_init(backends.data(), ma_uint32(backends.size()), &contextConfig, &d->context) != MA_SUCCESS) {
        d->error = "cannot initialise audio output";
        return;
    }
    d->contextReady = true;
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;
    config.playback.channels = 2;
    config.sampleRate = Rate;
    config.dataCallback = &Impl::dataCallback;
    config.pUserData = d.get();
    if (ma_device_init(&d->context, &config, &d->device) != MA_SUCCESS) {
        d->error = "cannot open the audio device";
        return;
    }
    d->deviceReady = true;
    // The Null backend's device stays stopped: tests mix by hand.
    if (backend != AudioEngine::Backend::Null && ma_device_start(&d->device) != MA_SUCCESS)
        d->error = "cannot start the audio device";
}

AudioMixer::~AudioMixer()
{
    if (d->deviceReady)
        ma_device_uninit(&d->device);
    if (d->contextReady)
        ma_context_uninit(&d->context);
}

bool AudioMixer::isAvailable() const
{
    return d->deviceReady;
}

std::string AudioMixer::errorString() const
{
    return d->error;
}

bool AudioMixer::play(std::shared_ptr<const Sound> sound, int owner, int channel, float volume, float pan, bool loop,
                      bool lowPriority, float speed)
{
    if (!d->deviceReady || !sound || sound->frames() == 0)
        return false;
    std::lock_guard lock(d->mutex);
    if (channel >= 0) {
        auto busy = std::find_if(d->voices.begin(), d->voices.end(),
                                 [&](const Voice& v) { return v.owner == owner && v.channel == channel; });
        if (busy != d->voices.end()) {
            if (lowPriority)
                return true;
            d->voices.erase(busy);
        }
    }
    Voice v;
    v.sound = std::move(sound);
    v.owner = owner;
    v.channel = channel;
    v.volume = std::max(0.0f, volume);
    v.loop = loop;
    v.speed = std::clamp(double(speed), 0.01, 8.0);
    setGains(v, pan);
    d->voices.push_back(std::move(v));
    return true;
}

void AudioMixer::stop(int owner, int channel)
{
    std::lock_guard lock(d->mutex);
    d->voices.erase(
        std::remove_if(d->voices.begin(), d->voices.end(),
                       [&](const Voice& v) { return v.owner == owner && (channel < 0 || v.channel == channel); }),
        d->voices.end());
}

void AudioMixer::stopAll()
{
    std::lock_guard lock(d->mutex);
    d->voices.clear();
}

void AudioMixer::setPan(int owner, int channel, float pan)
{
    std::lock_guard lock(d->mutex);
    for (Voice& v : d->voices) {
        if (v.owner == owner && (channel < 0 || v.channel == channel))
            setGains(v, pan);
    }
}

int AudioMixer::voices() const
{
    std::lock_guard lock(d->mutex);
    return int(d->voices.size());
}

std::vector<float> AudioMixer::mix(std::size_t frames)
{
    std::vector<float> out(frames * 2);
    d->render(out.data(), frames);
    return out;
}

} // namespace OFS
