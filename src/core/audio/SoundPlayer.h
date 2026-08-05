#pragma once

#include "core/CoreExport.h"
#include "core/audio/AudioEngine.h"

#include <QObject>
#include <QString>
#include <QTimer>

#include <atomic>
#include <memory>

namespace OFS {

// Editor playback: an AudioEngine that reports through Qt signals. finished() only comes when
// playback ends by itself. OFS_AUDIO_BACKEND=null plays to a silent device for tests.
class OFS_CORE_EXPORT SoundPlayer : public QObject
{
    Q_OBJECT

public:
    using State = AudioEngine::State;

    explicit SoundPlayer(QObject* parent = nullptr);
    explicit SoundPlayer(AudioEngine::Backend backend, QObject* parent = nullptr);
    ~SoundPlayer() override;

    bool isAvailable() const;
    QString errorString() const;
    QString backendName() const;

    bool play(std::shared_ptr<const Formats::SampleBuffer> buffer, std::size_t startFrame = 0);
    // Repeats `loop` until stop(). Play with loop of the whole sound passes Formats::kAllFrames.
    bool playLoop(std::shared_ptr<const Formats::SampleBuffer> buffer, Formats::SampleRange loop,
                  std::size_t startFrame = 0);
    void pause();
    bool resume();
    void stop();

    State state() const;
    bool isPlaying() const { return state() == State::Playing; }
    qint64 position() const;

    void setVolume(float volume);
    float volume() const;

    int positionInterval() const { return m_timer.interval(); }
    void setPositionInterval(int milliseconds) { m_timer.setInterval(milliseconds); }

Q_SIGNALS:
    void stateChanged(OFS::AudioEngine::State state);
    void positionChanged(qint64 frame);
    void finished();
    void deviceLost();

private:
    void handleEvent(AudioEngine::Event event);
    void updateState();

    std::unique_ptr<AudioEngine> m_engine;
    QTimer m_timer;
    State m_state = State::Stopped;
    // Bumped by play() and stop() so engine events of an earlier playback are ignored.
    std::atomic<int> m_generation{0};
};

} // namespace OFS
