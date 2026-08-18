#pragma once

#include "core/CoreExport.h"
#include "core/sprites/SpriteDocument.h"

#include <QList>
#include <QPoint>
#include <QString>
#include <QStringList>

namespace OFS {

// PNG, GIF, PCX or BMP by suffix. GIF and PCX hold indexed images only.
OFS_CORE_EXPORT bool writeImageFile(const QString& path, const Formats::Image& image, QString* error = nullptr);

OFS_CORE_EXPORT QStringList saveSpriteImages(const SpriteDocument& doc, const QList<int>& sprites, const QString& path,
                                             const QString& suffix, QString* error = nullptr);

struct OFS_CORE_EXPORT AlignedImages
{
    QList<Formats::Image> images;
    QPoint axis;
};
OFS_CORE_EXPORT AlignedImages alignSprites(const SpriteDocument& doc, const QList<int>& sprites);
OFS_CORE_EXPORT QStringList saveAlignedImages(const SpriteDocument& doc, const QList<int>& sprites,
                                              const AlignedImages& aligned, const QString& path, const QString& suffix,
                                              QString* error = nullptr);
// axis.txt: "X axis: 18" CR LF "Y axis: 91".
OFS_CORE_EXPORT QByteArray axisFileText(const QPoint& axis);

} // namespace OFS
