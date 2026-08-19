#pragma once

#include "core/CoreExport.h"
#include "core/sprites/SpriteDocument.h"

#include <QList>
#include <QString>

#include <functional>

namespace OFS {

// PNG, GIF, BMP and PCX keep their palette and exact indexes. Other formats Qt reads come as RGBA.
OFS_CORE_EXPORT Formats::Image loadImageFile(const QString& path, QString* error = nullptr);

// "1000_2.png", "1000-2.pcx" or "1000,2.bmp".
OFS_CORE_EXPORT bool identifiersFromFileName(const QString& fileName, int* group, int* index);

struct OFS_CORE_EXPORT AddSpritesOptions
{
    bool detectFromFileName = false;
    int group = 0;
    int index = 0;
    int indexStep = 1;
    // -1 uses (axisX, axisY). 0..8 go from top-left to bottom-right of the image, row by row.
    int axisAnchor = -1;
    int axisX = 0;
    int axisY = 0;
    enum class Palette
    {
        Own,
        Force,    // pixels remapped to the nearest colors
        Exchange, // pixels kept as they are
    };
    Palette palette = Palette::Own;
    int paletteGroup = 0;
    int paletteIndex = 0;
    bool reversedPalette = false;
    enum class Crop
    {
        None,
        BeforeAxis,
        AfterAxis,
    };
    Crop crop = Crop::None;
    enum class Share
    {
        Identical,
        Never,
        Ask,
    };
    Share share = Share::Identical;

    // Group for file names without identifiers. 7777 if unset.
    std::function<int(const QString& fileName)> askGroup;
    enum class Conflict
    {
        NextFree,
        NextFreeForAll,
        Skip,
        AsIs,
        AsIsForAll,
    };
    std::function<Conflict(int group, int index)> askConflict;
    std::function<bool(const QString& fileName, int existing)> askShare;
};

struct OFS_CORE_EXPORT ImageToAdd
{
    QString fileName;
    Formats::Image image;
};

OFS_CORE_EXPORT QList<int> addSprites(SpriteDocument& doc, int after, const QList<ImageToAdd>& images,
                                      const AddSpritesOptions& options);

} // namespace OFS
