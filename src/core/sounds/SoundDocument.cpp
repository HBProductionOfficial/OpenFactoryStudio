#include "core/sounds/SoundDocument.h"

#include <QCoreApplication>

#include <algorithm>

namespace OFS {

using namespace Formats;

SoundDocument SoundDocument::fromSnd(const SndDocument& snd)
{
    SoundDocument doc;
    doc.m_header.versionBytes = snd.versionBytes;
    doc.m_header.comment = snd.comment;
    for (const SndSound& sound : snd.sounds)
        doc.m_sounds << SoundEntry{sound.group, sound.number, std::make_shared<const Bytes>(sound.data)};
    return doc;
}

SndDocument SoundDocument::toSnd() const
{
    SndDocument snd = m_header;
    snd.sounds.clear();
    for (const SoundEntry& sound : m_sounds)
        snd.sounds.push_back(SndSound{sound.group, sound.number, sound.data ? *sound.data : Bytes()});
    return snd;
}

int SoundDocument::find(int group, int number) const
{
    for (int i = 0; i < count(); ++i) {
        if (m_sounds.at(i).group == group && m_sounds.at(i).number == number)
            return i;
    }
    return -1;
}

namespace SoundEdits {

Result<DecodedAudio> decode(const SoundEntry& sound)
{
    if (!sound.data || sound.data->empty())
        return Error{QCoreApplication::translate("OFS::SoundEdits", "The sound has no data.").toStdString()};
    return decodeAudio(ByteSpan(sound.data->data(), sound.data->size()));
}

bool setSamples(SoundDocument& doc, int index, const SampleBuffer& samples, int bitsPerSample, QString* error)
{
    if (index < 0 || index >= doc.count())
        return false;
    EncodeOptions options;
    options.format = AudioFileFormat::Wav;
    options.bitsPerSample = bitsPerSample == 8 ? 8 : 16;
    auto bytes = encodeAudio(samples, options);
    if (!bytes) {
        if (error)
            *error = QString::fromStdString(bytes.error().message);
        return false;
    }
    doc.sounds()[index].data = std::make_shared<const Bytes>(std::move(bytes.value()));
    return true;
}

Bytes importData(const Bytes& file, QString* error)
{
    const ByteSpan span(file.data(), file.size());
    const AudioFileFormat format = detectAudioFormat(span);
    if (format == AudioFileFormat::Wav)
        return file;
    auto decoded = decodeAudio(span);
    if (!decoded) {
        if (error)
            *error = QString::fromStdString(decoded.error().message);
        return {};
    }
    EncodeOptions options;
    options.bitsPerSample = 16;
    auto bytes = encodeAudio(decoded.value().buffer, options);
    if (!bytes) {
        if (error)
            *error = QString::fromStdString(bytes.error().message);
        return {};
    }
    return std::move(bytes.value());
}

void setIdentifiers(SoundDocument& doc, int index, int group, int number)
{
    if (index < 0 || index >= doc.count())
        return;
    doc.sounds()[index].group = group;
    doc.sounds()[index].number = number;
}

void remove(SoundDocument& doc, QList<int> indexes)
{
    std::sort(indexes.begin(), indexes.end(), std::greater<>());
    indexes.erase(std::unique(indexes.begin(), indexes.end()), indexes.end());
    for (int i : std::as_const(indexes)) {
        if (i >= 0 && i < doc.count())
            doc.sounds().removeAt(i);
    }
}

int duplicate(SoundDocument& doc, int index)
{
    if (index < 0 || index >= doc.count())
        return -1;
    const SoundEntry copy = doc.sounds().at(index);
    doc.sounds().insert(index + 1, copy);
    return index + 1;
}

int insert(SoundDocument& doc, int after, int group, int number, Bytes data)
{
    const int at = after >= 0 && after < doc.count() ? after + 1 : doc.count();
    doc.sounds().insert(at, SoundEntry{group, number, std::make_shared<const Bytes>(std::move(data))});
    return at;
}

} // namespace SoundEdits

} // namespace OFS
