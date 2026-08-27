#include "core/sounds/SoundTransfer.h"

#include "core/sprites/SpriteImages.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSaveFile>

#include <algorithm>

namespace OFS {

using namespace Formats;

namespace {

QString tr(const char* text)
{
    return QCoreApplication::translate("OFS::SoundTransfer", text);
}

bool writeFile(const QString& path, const QByteArray& data, QString* error)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        if (error)
            *error = QStringLiteral("%1: %2").arg(QDir::toNativeSeparators(path), file.errorString());
        return false;
    }
    return true;
}

QByteArray toQByteArray(const Bytes& bytes)
{
    return QByteArray(reinterpret_cast<const char*>(bytes.data()), qsizetype(bytes.size()));
}

Bytes readBytes(const QString& path, QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("%1: %2").arg(QDir::toNativeSeparators(path), file.errorString());
        return {};
    }
    const QByteArray data = file.readAll();
    const auto* begin = reinterpret_cast<const std::byte*>(data.constData());
    return Bytes(begin, begin + data.size());
}

QString soundFileName(const SoundEntry& sound)
{
    return QStringLiteral("%1-%2.wav").arg(sound.group).arg(sound.number);
}

} // namespace

bool exportSounds(const SoundDocument& doc, const QList<int>& sounds, const QString& listPath, SoundListFormat format,
                  QString* error)
{
    const QFileInfo list(listPath);
    const QDir dir = list.dir();
    QByteArray text;
    if (format == SoundListFormat::SndMaker)
        text += list.completeBaseName().toUtf8() + '\n';
    for (int index : sounds) {
        if (index < 0 || index >= doc.count())
            continue;
        const SoundEntry& sound = doc.sounds().at(index);
        const QString file = soundFileName(sound);
        if (!writeFile(dir.filePath(file), sound.data ? toQByteArray(*sound.data) : QByteArray(), error))
            return false;
        if (format == SoundListFormat::Ffe)
            text += QStringLiteral("\n[SoundDef]\ngroup = %1\nsample = %2\nfile = %3\n")
                        .arg(sound.group)
                        .arg(sound.number)
                        .arg(file)
                        .toUtf8();
        else
            text += QStringLiteral("%1\n%2\n%3\n").arg(file).arg(sound.group).arg(sound.number).toUtf8();
    }
    return writeFile(listPath, text, error);
}

QList<int> importFfe(SoundDocument& doc, int after, const QString& path, const SoundImportOptions& options,
                     QString* error)
{
    auto fail = [error](const QString& message) {
        if (error)
            *error = message;
        return QList<int>();
    };
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return fail(file.errorString());
    const QString name = QFileInfo(path).fileName();
    const QDir dir = QFileInfo(path).dir();
    const QStringList lines = QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'));
    // The list first: nothing is added when it is malformed.
    struct Section
    {
        QString kind;
        QHash<QString, QString> values;
        int line = 0;
    };
    QList<Section> sections;
    for (int n = 0; n < lines.size(); ++n) {
        QString line = lines.at(n);
        if (const qsizetype comment = line.indexOf(QLatin1Char(';')); comment >= 0)
            line.truncate(comment);
        line = line.trimmed();
        if (line.isEmpty())
            continue;
        if (line.startsWith(QLatin1Char('['))) {
            if (!line.endsWith(QLatin1Char(']')))
                return fail(tr("%1, line %2: the section name is not closed.").arg(name).arg(n + 1));
            sections << Section{line.mid(1, line.size() - 2).trimmed().toLower(), {}, n + 1};
            continue;
        }
        const qsizetype equals = line.indexOf(QLatin1Char('='));
        if (equals <= 0)
            return fail(tr("%1, line %2: expected 'name = value'.").arg(name).arg(n + 1));
        if (!sections.isEmpty())
            sections.last().values.insert(line.left(equals).trimmed().toLower(), line.mid(equals + 1).trimmed());
    }
    struct Incoming
    {
        int group;
        int number;
        Bytes data;
    };
    QList<Incoming> incoming;
    for (const Section& section : std::as_const(sections)) {
        if (section.kind != QLatin1String("sounddef"))
            continue;
        QString readError;
        Bytes data = readBytes(dir.filePath(section.values.value(QStringLiteral("file"))), &readError);
        if (data.empty())
            return fail(tr("%1, line %2: %3").arg(name).arg(section.line).arg(readError));
        data = SoundEdits::importData(data, &readError);
        if (data.empty())
            return fail(tr("%1, line %2: %3").arg(name).arg(section.line).arg(readError));
        incoming << Incoming{section.values.value(QStringLiteral("group")).toInt(),
                             section.values.value(QStringLiteral("sample")).toInt(), std::move(data)};
    }
    SoundDocument edited = doc;
    QList<int> added;
    int position = after;
    bool replaceAll = false;
    bool skipAll = false;
    for (Incoming& sound : incoming) {
        const int existing = edited.find(sound.group, sound.number);
        if (existing >= 0) {
            SoundConflict answer = replaceAll            ? SoundConflict::Replace
                                   : skipAll             ? SoundConflict::Skip
                                   : options.askConflict ? options.askConflict(sound.group, sound.number)
                                                         : SoundConflict::Insert;
            replaceAll = replaceAll || answer == SoundConflict::ReplaceAll;
            skipAll = skipAll || answer == SoundConflict::SkipAll;
            if (answer == SoundConflict::Replace || answer == SoundConflict::ReplaceAll) {
                edited.sounds()[existing].data = std::make_shared<const Bytes>(std::move(sound.data));
                continue;
            }
            if (answer == SoundConflict::Skip || answer == SoundConflict::SkipAll)
                continue;
        }
        position = SoundEdits::insert(edited, position, sound.group, sound.number, std::move(sound.data));
        added << position;
    }
    doc = edited;
    return added;
}

QList<int> addSoundFiles(SoundDocument& doc, int after, const QStringList& files, const SoundAddOptions& options,
                         QStringList* errors)
{
    QList<int> added;
    int position = after;
    int number = options.number;
    for (const QString& path : files) {
        QString error;
        Bytes data = readBytes(path, &error);
        if (!data.empty())
            data = SoundEdits::importData(data, &error);
        if (data.empty()) {
            if (errors)
                *errors << QStringLiteral("%1: %2").arg(QFileInfo(path).fileName(), error);
            continue;
        }
        int group = options.group;
        int index = number;
        if (!options.detect || !identifiersFromFileName(path, &group, &index)) {
            group = options.group;
            index = number;
            number += std::max(1, options.step);
        }
        position = SoundEdits::insert(doc, position, group, index, std::move(data));
        added << position;
    }
    return added;
}

QStringList saveSounds(const SoundDocument& doc, const QList<int>& sounds, const QString& path, QString* error)
{
    QStringList written;
    const QFileInfo info(path);
    const QString suffix = info.suffix().isEmpty() ? QStringLiteral("wav") : info.suffix().toLower();
    for (int index : sounds) {
        if (index < 0 || index >= doc.count())
            continue;
        const SoundEntry& sound = doc.sounds().at(index);
        const QString target = sounds.size() == 1 ? (info.suffix().isEmpty() ? path + QStringLiteral(".wav") : path)
                                                  : info.dir().filePath(QStringLiteral("%1_%2-%3.%4")
                                                                            .arg(info.completeBaseName())
                                                                            .arg(sound.group)
                                                                            .arg(sound.number)
                                                                            .arg(suffix));
        QByteArray data;
        const bool storedWav =
            sound.data && detectAudioFormat(ByteSpan(sound.data->data(), sound.data->size())) == AudioFileFormat::Wav;
        if (suffix == QLatin1String("wav") && storedWav) {
            data = toQByteArray(*sound.data);
        } else {
            auto decoded = SoundEdits::decode(sound);
            if (!decoded) {
                if (error)
                    *error = QString::fromStdString(decoded.error().message);
                return written;
            }
            EncodeOptions encode;
            encode.format = suffix == QLatin1String("ogg")    ? AudioFileFormat::OggVorbis
                            : suffix == QLatin1String("flac") ? AudioFileFormat::Flac
                                                              : AudioFileFormat::Wav;
            encode.bitsPerSample = decoded.value().bitsPerSample == 8 && encode.format == AudioFileFormat::Wav ? 8 : 16;
            auto bytes = encodeAudio(decoded.value().buffer, encode);
            if (!bytes) {
                if (error)
                    *error = QString::fromStdString(bytes.error().message);
                return written;
            }
            data = toQByteArray(bytes.value());
        }
        if (!writeFile(target, data, error))
            return written;
        written << target;
    }
    return written;
}

} // namespace OFS
