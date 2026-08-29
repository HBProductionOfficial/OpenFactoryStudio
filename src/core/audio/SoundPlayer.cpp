#include "core/audio/SoundPlayer.h"

namespace OFS {

SoundPlayer::SoundPlayer(QObject* parent)
    : SoundPlayer(qEnvironmentVariable("OFS_AUDIO_BACKEND") == QLatin1String("null") ? AudioEngine::Backend::Null
                                                                                     : AudioEngine::Backend::System,
                  parent)
{}

SoundPlayer::SoundPlayer(AudioEngine::Backend backend, QObject* parent)
    : QObject(parent)
    , m_engine(std::make_unique<AudioEngine>(backend))
    , m_timer(this)
{
    m_timer.setInterval(30);
    connect(&m_timer, &QTimer::timeout, this, [this] { Q_EMIT positionChanged(position()); });
    m_engine->setEventCallback([this](AudioEngine::Event event) {
        // Audio thread: post the event to this object's thread, tagged with its playback.
        const int generation = m_generation.load();
        QMetaObject::invokeMethod(
            this,
            [this, event, generation] {
                if (generation == m_generation.load())
                    handleEvent(event);
            },
            Qt::QueuedConnection);
    });
}

SoundPlayer::~SoundPlayer()
{
    // Close the device, which waits for the audio thread, before anything else goes away.
    m_engine.reset();
}

bool SoundPlayer::isAvailable() const
{
    return m_engine->isAvailable();
}

QString SoundPlayer::errorString() const
{
    return QString::fromStdString(m_engine->errorString());
}

QString SoundPlayer::backendName() const
{
    return QString::fromStdString(m_engine->backendName());
}

bool SoundPlayer::play(std::shared_ptr<const Formats::SampleBuffer> buffer, std::size_t startFrame)
{
    ++m_generation;
    const bool ok = m_engine->play(std::move(buffer), startFrame);
    updateState();
    return ok;
}

bool SoundPlayer::playLoop(std::shared_ptr<const Formats::SampleBuffer> buffer, Formats::SampleRange loop,
                           std::size_t startFrame)
{
    ++m_generation;
    const bool ok = m_engine->play(std::move(buffer), startFrame, loop);
    updateState();
    return ok;
}

void SoundPlayer::pause()
{
    m_engine->pause();
    updateState();
}

bool SoundPlayer::resume()
{
    const bool ok = m_engine->resume();
    updateState();
    return ok;
}

void SoundPlayer::stop()
{
    ++m_generation;
    m_engine->stop();
    updateState();
}

SoundPlayer::State SoundPlayer::state() const
{
    return m_engine->state();
}

qint64 SoundPlayer::position() const
{
    return static_cast<qint64>(m_engine->position());
}

void SoundPlayer::setVolume(float volume)
{
    m_engine->setVolume(volume);
}

float SoundPlayer::volume() const
{
    return m_engine->volume();
}

void SoundPlayer::handleEvent(AudioEngine::Event event)
{
    // The engine stopped by itself. stop() releases the device and the samples.
    m_engine->stop();
    updateState();
    if (event == AudioEngine::Event::Finished)
        Q_EMIT finished();
    else
        Q_EMIT deviceLost();
}

void SoundPlayer::updateState()
{
    const State current = m_engine->state();
    if (current == State::Playing)
        m_timer.start();
    else
        m_timer.stop();
    Q_EMIT positionChanged(position());
    if (current != m_state) {
        m_state = current;
        Q_EMIT stateChanged(current);
    }
}

} // namespace OFS
