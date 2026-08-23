#pragma once

#include "core/CoreExport.h"
#include "core/sprites/SpriteImages.h"

#include <QList>
#include <QString>

#include <functional>

namespace OFS {

enum class SpriteListFormat
{
    Ffe,       // Fighter Factory Export (.ffe)
    SprMaker2, // Elecbyte SprMaker 2 (.def)
};

// Writes the list, one "<group>-<index>.png" per distinct image and one "<group>-<index>.act" per
// palette. Linked sprites refer to the same PNG.
OFS_CORE_EXPORT bool exportSprites(const SpriteDocument& doc, const QList<int>& sprites, const QString& listPath,
                                   SpriteListFormat format, QString* error = nullptr);

// Identical palettes are reused only when the list says noDuplicatePal = 1. Sprites listing the
// same image file share it. On a malformed list nothing is added and *error tells the line.
OFS_CORE_EXPORT QList<int> importFfe(SpriteDocument& doc, int after, const QString& path,
                                     const AddSpritesOptions& options, QString* error = nullptr);
struct OFS_CORE_EXPORT ProjectImportItem
{
    int source = 0;
    int group = 0;
    int number = 0;
};
struct OFS_CORE_EXPORT ProjectImportOptions
{
    enum class Palette
    {
        ImageOwn,
        Force,
        Exchange,
    };
    enum class Conflict
    {
        Ask,
        Replace,
        Skip,
        AsIs,
    };
    Palette palette = Palette::ImageOwn;
    int paletteIndex = 0;
    Conflict conflict = Conflict::Ask;
    std::function<bool(int group, int number)> askReplace;
};
struct OFS_CORE_EXPORT ProjectImportResult
{
    QList<int> added;
    int replaced = 0;
};
OFS_CORE_EXPORT ProjectImportResult importFromProject(SpriteDocument& doc, int after, const SpriteDocument& source,
                                                      const QList<ProjectImportItem>& items,
                                                      const ProjectImportOptions& options);

} // namespace OFS
