#include "engines/mugen/emulator/BackgroundMusic.h"

#include "engines/mugen/runtime/Files.h"
#include "engines/mugen/runtime/IniText.h"
#include "formats/audio/AudioCodec.h"

#include <QDir>
#include <QFileInfo>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <filesystem>
#include <limits>

namespace OFS::Mugen {

using namespace Runtime;

QString BackgroundMusic::find(const QString& name, const QStringList& folders)
{
    const QString cleaned = QDir::fromNativeSeparators(unquote(name).trimmed());
    if (cleaned.isEmpty())
        return QString();
    for (const QString& folder : folders) {
        const QString candidate = QDir(folder).filePath(cleaned);
        if (QFileInfo(candidate).isFile())
            return QDir::cleanPath(candidate);
    }
    return QString();
}

float BackgroundMusic::engineVolume(const QString& engineDir)
{
    const IniText config = IniText::parse(readText(QDir(engineDir).filePath(QStringLiteral("data/mugen.cfg"))));
    const IniSection* sound = config.section(QStringLiteral("Sound"));
    auto percent = [sound](const char* key) {
        bool ok = false;
        const float value = sound ? sound->value(QLatin1String(key)).toFloat(&ok) : 0;
        return ok ? std::clamp(value / 100, 0.0f, 1.0f) : 1.0f;
    };
    return percent("MasterVolume") * percent("BGMVolume");
}

void BackgroundMusic::play(const Track& next)
{
    const bool same = !next.file.isEmpty() && next.file == m_track.file && next.loop && m_track.loop;
    if (same) {
        m_volumeChanged = m_volumeChanged || next.volume != m_track.volume;
        m_track.volume = next.volume;
        return;
    }
    m_track = next;
    m_stopping = true;
    m_starting = !next.file.isEmpty();
    m_volumeChanged = false;
    if (!m_starting)
        return;
    const std::filesystem::path path(next.file.toStdWString());
    m_decoding = QtConcurrent::run([path]() -> std::shared_ptr<const Formats::SampleBuffer> {
        // Kept as 16-bit samples, which take half the memory of floats for a long song.
        Formats::DecodeOptions options;
        options.sampleType = Formats::SampleType::Int16;
        auto decoded = Formats::loadAudio(path, options);
        if (!decoded)
            return nullptr;
        return std::make_shared<const Formats::SampleBuffer>(std::move(decoded.value().buffer));
    });
}

void BackgroundMusic::update(AudioMixer& mixer, bool paused)
{
    if (m_stopping) {
        mixer.stopMusic();
        m_stopping = false;
    }
    if (m_starting && m_decoding.isFinished()) {
        m_starting = false;
        AudioMixer::MusicOptions options;
        options.volume = m_track.volume;
        options.loop = m_track.loop;
        constexpr qint64 most = std::numeric_limits<qint64>::max();
        options.loopStart = std::size_t(std::clamp<qint64>(m_track.loopStart, 0, most));
        options.loopEnd = std::size_t(std::clamp<qint64>(m_track.loopEnd, 0, most));
        if (std::shared_ptr<const Formats::SampleBuffer> samples = m_decoding.result())
            mixer.playMusic(std::move(samples), options);
        m_decoding = {};
    }
    if (m_volumeChanged) {
        mixer.setMusicVolume(m_track.volume);
        m_volumeChanged = false;
    }
    mixer.setMusicPaused(paused);
}

void BackgroundMusic::waitUntilDecoded() const
{
    if (m_starting) {
        QFuture<std::shared_ptr<const Formats::SampleBuffer>> decoding = m_decoding;
        decoding.waitForFinished();
    }
}

} // namespace OFS::Mugen
