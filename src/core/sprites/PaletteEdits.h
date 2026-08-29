#pragma once

#include "core/CoreExport.h"
#include "core/sprites/SpriteDocument.h"
#include "core/syntax/Issue.h"
#include "formats/util/Result.h"

#include <QList>
#include <QString>
#include <QStringList>

namespace OFS::PaletteEdits {

// Palettes panel operations, used like SpriteEdits. Palettes are shared records, so a color edit
// makes a new record and moves every sprite that used the old one to it.

OFS_CORE_EXPORT bool isIndexed(const SpriteEntry& entry);
OFS_CORE_EXPORT QList<int> users(const SpriteDocument& doc, int index);
OFS_CORE_EXPORT int paletteOf(const SpriteDocument& doc, int sprite);

OFS_CORE_EXPORT void setColors(SpriteDocument& doc, int index, Formats::Palette colors);
OFS_CORE_EXPORT void setIdentifiers(SpriteDocument& doc, int index, int group, int number);
OFS_CORE_EXPORT int insert(SpriteDocument& doc, int after, int group, int number, Formats::Palette colors);
OFS_CORE_EXPORT Formats::Palette emptyColors();

OFS_CORE_EXPORT void remove(SpriteDocument& doc, int index, int replacement);
OFS_CORE_EXPORT int removeDuplicates(SpriteDocument& doc);
// Keeps character palettes (group 1) even when unused.
OFS_CORE_EXPORT int removeUnused(SpriteDocument& doc);
OFS_CORE_EXPORT void clear(SpriteDocument& doc);

OFS_CORE_EXPORT void switchCount(SpriteDocument& doc, int index);
OFS_CORE_EXPORT void moveColor(SpriteDocument& doc, int index, int from, int to);
OFS_CORE_EXPORT void exchange(SpriteDocument& doc, int index, QList<int> slots);
OFS_CORE_EXPORT void fill(SpriteDocument& doc, int index, const QList<int>& slots, Formats::Rgba color);
OFS_CORE_EXPORT void applyToSprites(SpriteDocument& doc, int index, const QList<int>& sprites);

OFS_CORE_EXPORT IssueList issues(const SpriteDocument& doc);

// ACT files store colors in reverse order. Files with up to 32 colors give 32, others 256. Slot 0
// is transparent.
OFS_CORE_EXPORT Formats::Result<Formats::Palette> readFile(const QString& path, bool reversed = false);
// Several palettes go to "<path base>_<group>-<index>.act". ACT files are 768 bytes in reverse
// order, without alpha.
OFS_CORE_EXPORT QStringList saveAct(const SpriteDocument& doc, const QList<int>& palettes, const QString& path,
                                    QString* error = nullptr);

} // namespace OFS::PaletteEdits
