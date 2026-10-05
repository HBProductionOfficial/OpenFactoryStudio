#pragma once

#include "core/audio/AudioMixer.h"
#include "formats/audio/SampleBuffer.h"

#include <QFuture>
#include <QString>
#include <QStringList>

#include <memory>

namespace OFS::Mugen {

// The music behind a stage, motif screen or storyboard, decoded on a worker thread. WAV, Ogg Vorbis
// and MP3 play. MIDI, module and CD audio tracks stay silent.
class BackgroundMusic
{
public:
    struct Track
    {
        QString file;
        float volume = 1; // times the engine's music volume
        bool loop = true;
        qint64 loopStart = 0; // in frames of the file (0 to 0: all of it)
        qint64 loopEnd = 0;
    };

    static QString find(const QString& name, const QStringList& folders);
    // mugen.cfg's [Sound] MasterVolume times BGMVolume, 0 to 1.
    static float engineVolume(const QString& engineDir);

    // A looping track that already plays the same file goes on at the new volume.
    void play(const Track& next);
    void stop() { play(Track()); }
    const Track& track() const { return m_track; }
    void update(AudioMixer& mixer, bool paused = false);
    void waitUntilDecoded() const;

private:
    Track m_track;
    QFuture<std::shared_ptr<const Formats::SampleBuffer>> m_decoding;
    bool m_starting = false;
    bool m_stopping = false;
    bool m_volumeChanged = false;
};

} // namespace OFS::Mugen
