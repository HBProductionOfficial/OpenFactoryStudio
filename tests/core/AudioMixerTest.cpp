#include "core/audio/AudioMixer.h"

#include <QTest>

#include <cmath>

using namespace OFS;
using Formats::SampleBuffer;
using Formats::SampleType;

namespace {

std::shared_ptr<const AudioMixer::Sound> level(float left, float right, std::size_t frames)
{
    SampleBuffer buffer(SampleType::Float32, 2, AudioMixer::Rate, frames);
    for (std::size_t f = 0; f < frames; ++f) {
        buffer.setSample(f, 0, left);
        buffer.setSample(f, 1, right);
    }
    return AudioMixer::prepare(buffer);
}

bool near(float a, float b)
{
    return std::abs(a - b) < 1e-4f;
}

} // namespace

class AudioMixerTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void prepare()
    {
        SampleBuffer mono(SampleType::Int16, 1, AudioMixer::Rate / 2, 1000);
        for (std::size_t f = 0; f < mono.frameCount(); ++f)
            mono.setSample(f, 0, 0.5f);
        const auto sound = AudioMixer::prepare(mono);
        QVERIFY(sound->frames() >= 1990 && sound->frames() <= 2010);
        QVERIFY(near(sound->samples[1000], 0.5f));
        QVERIFY(near(sound->samples[1001], 0.5f));
        QCOMPARE(AudioMixer::prepare(SampleBuffer())->frames(), std::size_t(0));
    }

    void mixes()
    {
        AudioMixer mixer(AudioEngine::Backend::Null);
        QVERIFY2(mixer.isAvailable(), mixer.errorString().c_str());
        const auto a = level(0.25f, 0.25f, 100);
        const auto b = level(0.125f, 0.5f, 50);

        QVERIFY(mixer.play(a, 0, -1));
        QVERIFY(mixer.play(b, 0, -1));
        QCOMPARE(mixer.voices(), 2);
        std::vector<float> out = mixer.mix(80);
        QVERIFY(near(out[0], 0.375f));
        QVERIFY(near(out[1], 0.75f));
        QVERIFY(near(out[60 * 2], 0.25f));
        QCOMPARE(mixer.voices(), 1);
        out = mixer.mix(40);
        QVERIFY(near(out[19 * 2], 0.25f));
        QVERIFY(near(out[20 * 2], 0.f));
        QCOMPARE(mixer.voices(), 0);

        QVERIFY(!mixer.play(nullptr, 0, -1));
        QVERIFY(!mixer.play(level(1, 1, 0), 0, -1));
    }

    void channels()
    {
        AudioMixer mixer(AudioEngine::Backend::Null);
        const auto a = level(0.25f, 0.25f, 1000);
        const auto b = level(0.5f, 0.5f, 1000);

        mixer.play(a, 1, 2);
        mixer.play(b, 1, 2);
        QCOMPARE(mixer.voices(), 1);
        QVERIFY(near(mixer.mix(1)[0], 0.5f));
        mixer.play(a, 1, 2, 1, 0, false, true);
        QVERIFY(near(mixer.mix(1)[0], 0.5f));
        mixer.play(a, 2, 2);
        QCOMPARE(mixer.voices(), 2);
        QVERIFY(near(mixer.mix(1)[0], 0.75f));

        mixer.play(a, 1, 3);
        mixer.play(a, 1, -1);
        QCOMPARE(mixer.voices(), 4);
        mixer.stop(1, 3);
        QCOMPARE(mixer.voices(), 3);
        mixer.stop(1, -1);
        QCOMPARE(mixer.voices(), 1);
        mixer.stopAll();
        QCOMPARE(mixer.voices(), 0);
    }

    void panVolumeSpeedLoop()
    {
        AudioMixer mixer(AudioEngine::Backend::Null);
        const auto a = level(0.5f, 0.5f, 100);

        mixer.play(a, 0, 0, 0.5f, -1);
        std::vector<float> out = mixer.mix(1);
        QVERIFY(near(out[0], 0.25f));
        QVERIFY(near(out[1], 0.f));
        mixer.setPan(0, 0, 1);
        out = mixer.mix(1);
        QVERIFY(near(out[0], 0.f));
        QVERIFY(near(out[1], 0.25f));
        mixer.stopAll();

        mixer.play(a, 0, -1, 1, 0, false, false, 2);
        out = mixer.mix(60);
        QVERIFY(near(out[49 * 2], 0.5f));
        QVERIFY(near(out[50 * 2], 0.f));
        QCOMPARE(mixer.voices(), 0);

        mixer.play(a, 0, 1, 1, 0, true);
        out = mixer.mix(250);
        QVERIFY(near(out[249 * 2], 0.5f));
        QCOMPARE(mixer.voices(), 1);
        mixer.stop(0, 1);
        QCOMPARE(mixer.voices(), 0);
    }
};

QTEST_GUILESS_MAIN(AudioMixerTest)
#include "AudioMixerTest.moc"
