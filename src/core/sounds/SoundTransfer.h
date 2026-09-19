#pragma once

#include "core/CoreExport.h"
#include "core/sounds/SoundDocument.h"

#include <QList>
#include <QString>
#include <QStringList>

#include <functional>

namespace OFS {

enum class SoundListFormat
{
    Ffe,      // Fighter Factory Export (.ffe)
    SndMaker, // Elecbyte SndMaker (.txt)
};

// Writes each sound as "<group>-<index>.wav" beside the list file, then the list.
OFS_CORE_EXPORT bool exportSounds(const SoundDocument& doc, const QList<int>& sounds, const QString& listPath,
                                  SoundListFormat format, QString* error = nullptr,
                                  const std::function<bool(int written)>& progress = {});

enum class SoundConflict
{
    Replace,
    ReplaceAll,
    Skip,
    SkipAll,
    Insert,
};
struct OFS_CORE_EXPORT SoundImportOptions
{
    std::function<SoundConflict(int group, int number)> askConflict;
};

OFS_CORE_EXPORT QList<int> importFfe(SoundDocument& doc, int after, const QString& path,
                                     const SoundImportOptions& options, QString* error = nullptr);

// `detect` takes the numbers from the file names ("700_5.wav" -> 700, 5).
struct OFS_CORE_EXPORT SoundAddOptions
{
    bool detect = false;
    int group = 0;
    int number = 0;
    int step = 1;
};
OFS_CORE_EXPORT QList<int> addSoundFiles(SoundDocument& doc, int after, const QStringList& files,
                                         const SoundAddOptions& options, QStringList* errors = nullptr);

// Several sounds go to "<path base>_<group>-<index>.<suffix>". The suffix picks the format.
OFS_CORE_EXPORT QStringList saveSounds(const SoundDocument& doc, const QList<int>& sounds, const QString& path,
                                       QString* error = nullptr);

} // namespace OFS
