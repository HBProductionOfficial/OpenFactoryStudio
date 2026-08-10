#include "core/audio/AudioEngine.h"
#include "core/audio/SoundPlayer.h"

#include <QSignalSpy>
#include <QTest>

#include <atomic>
#include <chrono>
#include <limits>
#include <thread>

using namespace OFS;
using Formats::SampleBuffer;
using Formats::SampleRange;
using Formats::SampleType;

namespace {

std::shared_ptr<const SampleBuffer> tone(SampleType type, std::uint16_t channels, std::uint32_t rate, double seconds)
{
    auto buffer = std::make_shared<SampleBuffer>(type, channels, rate, static_cast<std::size_t>(seconds * rate));
    for (std::size_t f = 0; f < buffer->frameCount(); ++f) {
        for (std::uint16_t c = 0; c < channels; ++c)
            buffer->setSample(f, c, (f / 20) % 2 ? 0.25f : -0.25f);
    }
    return buffer;
}

} // namespace

class AudioPlayerTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void playsToTheEnd()
    {
        SoundPlayer player(AudioEngine::Backend::Null);
        QVERIFY(player.isAvailable());
        QCOMPARE(player.backendName(), QStringLiteral("Null"));
        QSignalSpy finished(&player, &SoundPlayer::finished);
        QSignalSpy states(&player, &SoundPlayer::stateChanged);
        QSignalSpy positions(&player, &SoundPlayer::positionChanged);

        const auto buffer = tone(SampleType::Int16, 1, 22050, 0.3);
        QVERIFY2(player.play(buffer, 100), qPrintable(player.errorString()));
        QCOMPARE(player.state(), AudioEngine::State::Playing);
        QVERIFY(player.errorString().isEmpty());
        QCOMPARE(states.count(), 1);
        QVERIFY(player.position() >= 100);

        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 10000);
        QCOMPARE(player.state(), AudioEngine::State::Stopped);
        QCOMPARE(states.count(), 2);
        QCOMPARE(states.last().at(0).value<AudioEngine::State>(), AudioEngine::State::Stopped);
        QVERIFY(positions.count() >= 3);
        QCOMPARE(player.position(), qint64(100));
        QTest::qWait(100);
        QCOMPARE(finished.count(), 1);
    }

    void positionMovesWhilePlaying()
    {
        SoundPlayer player(AudioEngine::Backend::Null);
        player.setPositionInterval(10);
        QSignalSpy positions(&player, &SoundPlayer::positionChanged);
        const auto buffer = tone(SampleType::Float32, 2, 44100, 5.0);
        QVERIFY(player.play(buffer));
        QTRY_VERIFY_WITH_TIMEOUT(player.position() > 4410, 10000);
        QTRY_VERIFY_WITH_TIMEOUT(positions.count() > 3, 10000);
        QVERIFY(player.position() < qint64(buffer->frameCount()));
        player.stop();
        QCOMPARE(player.state(), AudioEngine::State::Stopped);
        QCOMPARE(player.position(), qint64(0));
    }

    void pauseKeepsThePosition()
    {
        SoundPlayer player(AudioEngine::Backend::Null);
        QSignalSpy finished(&player, &SoundPlayer::finished);
        const auto buffer = tone(SampleType::Int16, 2, 22050, 5.0);
        QVERIFY(player.play(buffer, 1000));
        QTRY_VERIFY_WITH_TIMEOUT(player.position() > 3000, 10000);
        player.pause();
        QCOMPARE(player.state(), AudioEngine::State::Paused);
        const qint64 paused = player.position();
        QTest::qWait(150);
        QCOMPARE(player.position(), paused);
        QVERIFY(player.resume());
        QCOMPARE(player.state(), AudioEngine::State::Playing);
        QTRY_VERIFY_WITH_TIMEOUT(player.position() > paused + 2000, 10000);
        player.stop();
        QCOMPARE(player.position(), qint64(1000));
        QVERIFY(!player.resume());
        QTest::qWait(50);
        QCOMPARE(finished.count(), 0);
    }

    void loopsUntilStopped()
    {
        SoundPlayer player(AudioEngine::Backend::Null);
        QSignalSpy finished(&player, &SoundPlayer::finished);
        const auto buffer = tone(SampleType::Int16, 1, 22050, 0.05);
        QVERIFY(player.playLoop(buffer, SampleRange{200, 500}, 0));
        QTest::qWait(400);
        QCOMPARE(player.state(), AudioEngine::State::Playing);
        QCOMPARE(finished.count(), 0);
        QVERIFY(player.position() >= 200 && player.position() <= 700);
        player.stop();

        QVERIFY(player.playLoop(buffer, Formats::kAllFrames));
        QTest::qWait(200);
        QCOMPARE(player.state(), AudioEngine::State::Playing);
        player.stop();
        QCOMPARE(finished.count(), 0);
    }

    void switchesFormatsAndSounds()
    {
        SoundPlayer player(AudioEngine::Backend::Null);
        QSignalSpy finished(&player, &SoundPlayer::finished);
        QVERIFY(player.play(tone(SampleType::Int16, 1, 11025, 2.0)));
        QVERIFY(player.play(tone(SampleType::Float32, 2, 48000, 0.2), 5000));
        QVERIFY(player.position() >= 5000);
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 10000);
        QTest::qWait(100);
        QCOMPARE(finished.count(), 1);
        QCOMPARE(player.position(), qint64(5000));
        QVERIFY(player.play(tone(SampleType::Int16, 1, 8000, 0.1), 999999));
        QVERIFY(player.position() < 800);
        player.stop();
        QCOMPARE(player.position(), qint64(0));
    }

    void rejectsNothingToPlay()
    {
        SoundPlayer player(AudioEngine::Backend::Null);
        QVERIFY(!player.play(nullptr));
        QVERIFY(!player.errorString().isEmpty());
        QVERIFY(!player.play(std::make_shared<SampleBuffer>(SampleType::Int16, 1, 22050)));
        QCOMPARE(player.state(), AudioEngine::State::Stopped);
        auto noRate = std::make_shared<SampleBuffer>(SampleType::Int16, 1, 0, 100);
        QVERIFY(!player.play(noRate));
        QVERIFY(player.play(tone(SampleType::Int16, 1, 22050, 0.1)));
        QVERIFY(player.errorString().isEmpty());
        player.stop();
    }

    void volumeIsClamped()
    {
        SoundPlayer player(AudioEngine::Backend::Null);
        QCOMPARE(player.volume(), 1.0f);
        player.setVolume(0.55f);
        QCOMPARE(player.volume(), 0.55f);
        player.setVolume(3.0f);
        QCOMPARE(player.volume(), 1.0f);
        player.setVolume(-1.0f);
        QCOMPARE(player.volume(), 0.0f);
        player.setVolume(std::numeric_limits<float>::quiet_NaN());
        QCOMPARE(player.volume(), 0.0f);
    }

    void engineWorksWithoutQt()
    {
        AudioEngine engine(AudioEngine::Backend::Null);
        std::atomic<int> finished{0};
        engine.setEventCallback([&finished](AudioEngine::Event event) {
            if (event == AudioEngine::Event::Finished)
                ++finished;
        });
        QVERIFY(engine.play(tone(SampleType::Float32, 1, 16000, 0.1)));
        QVERIFY(!engine.deviceName().empty());
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (finished.load() == 0 && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        QCOMPARE(finished.load(), 1);
        QCOMPARE(engine.state(), AudioEngine::State::Stopped);
        engine.stop();
        QCOMPARE(finished.load(), 1);
    }

    void destroyedWhilePlaying()
    {
        auto player = std::make_unique<SoundPlayer>(AudioEngine::Backend::Null);
        QVERIFY(player->play(tone(SampleType::Int16, 2, 44100, 10.0)));
        QTest::qWait(50);
        player.reset();
        QVERIFY(true);
    }

    void systemBackendFailsGracefully()
    {
        // Works with a real device or with none (CI).
        SoundPlayer player;
        if (!player.isAvailable()) {
            QVERIFY(!player.errorString().isEmpty());
            QVERIFY(!player.play(tone(SampleType::Int16, 1, 22050, 0.05)));
            QVERIFY(!player.errorString().isEmpty());
            return;
        }
        QVERIFY(!player.backendName().isEmpty());
        QVERIFY(player.backendName() != QStringLiteral("Null"));
        auto silence = std::make_shared<SampleBuffer>(SampleType::Int16, 1, 22050, 1102);
        if (player.play(silence)) {
            QCOMPARE(player.state(), AudioEngine::State::Playing);
            player.stop();
        } else {
            QVERIFY(!player.errorString().isEmpty());
            QCOMPARE(player.state(), AudioEngine::State::Stopped);
        }
    }
};

QTEST_GUILESS_MAIN(AudioPlayerTest)
#include "AudioPlayerTest.moc"
