#pragma once

#include "core/CoreExport.h"
#include "formats/audio/AudioCodec.h"
#include "formats/snd/SndDocument.h"

#include <QList>
#include <QString>

#include <memory>

namespace OFS {

// The data is shared between copies so undo snapshots stay cheap.
struct OFS_CORE_EXPORT SoundEntry
{
    int group = 0;
    int number = 0;
    std::shared_ptr<const Formats::Bytes> data;
};

// Sounds of an SND file. The header's version bytes and comment area are kept.
class OFS_CORE_EXPORT SoundDocument
{
public:
    static SoundDocument fromSnd(const Formats::SndDocument& snd);
    Formats::SndDocument toSnd() const;

    const QList<SoundEntry>& sounds() const { return m_sounds; }
    QList<SoundEntry>& sounds() { return m_sounds; }
    int count() const { return int(m_sounds.size()); }
    int find(int group, int number) const;

private:
    Formats::SndDocument m_header;
    QList<SoundEntry> m_sounds;
};

namespace SoundEdits {

// Bits per sample is 0 for lossy formats.
OFS_CORE_EXPORT Formats::Result<Formats::DecodedAudio> decode(const SoundEntry& sound);
OFS_CORE_EXPORT bool setSamples(SoundDocument& doc, int index, const Formats::SampleBuffer& samples, int bitsPerSample,
                                QString* error = nullptr);
OFS_CORE_EXPORT Formats::Bytes importData(const Formats::Bytes& file, QString* error = nullptr);
OFS_CORE_EXPORT void setIdentifiers(SoundDocument& doc, int index, int group, int number);
OFS_CORE_EXPORT void remove(SoundDocument& doc, QList<int> indexes);
OFS_CORE_EXPORT int duplicate(SoundDocument& doc, int index);
OFS_CORE_EXPORT int insert(SoundDocument& doc, int after, int group, int number, Formats::Bytes data);

} // namespace SoundEdits

} // namespace OFS
