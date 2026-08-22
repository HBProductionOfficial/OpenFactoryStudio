#pragma once

#include "core/CoreExport.h"
#include "core/sprites/SpriteDocument.h"
#include "core/syntax/Issue.h"
#include "formats/image/Resize.h"

#include <QList>
#include <QPoint>

namespace OFS::SpriteEdits {

// Sprites editor operations on a sprite document. Each one changes doc in place. Callers work on
// a copy and pass it to SpriteFile::apply() as one undo step. Positions index the sprite list.

OFS_CORE_EXPORT void setIdentifiers(SpriteDocument& doc, int index, int group, int number);
OFS_CORE_EXPORT void setAxis(SpriteDocument& doc, int index, int x, int y);
OFS_CORE_EXPORT void moveAxes(SpriteDocument& doc, const QList<int>& indexes, int dx, int dy);

// Moves the axis so a point of the image sits where the guide lines cross (guides is in units
// from the axis). horizontal: 0 left, 1 centre, 2 right. vertical: 0 top, 1 centre, 2 bottom.
OFS_CORE_EXPORT void alignToGuides(SpriteDocument& doc, int index, int horizontal, int vertical, const QPoint& guides);

OFS_CORE_EXPORT void remove(SpriteDocument& doc, QList<int> indexes);
OFS_CORE_EXPORT int removeGroup(SpriteDocument& doc, int group, int first = -1, int last = -1);
OFS_CORE_EXPORT int renameGroup(SpriteDocument& doc, int from, int to, int first = -1, int last = -1);

OFS_CORE_EXPORT int freeNumber(const SpriteDocument& doc, int group, int start = 0);
OFS_CORE_EXPORT int duplicate(SpriteDocument& doc, int index);

OFS_CORE_EXPORT void setImage(SpriteDocument& doc, int index, Formats::Image image);

OFS_CORE_EXPORT void crop(SpriteDocument& doc, const QList<int>& indexes);
OFS_CORE_EXPORT void resize(SpriteDocument& doc, const QList<int>& indexes, double widthScale, double heightScale,
                            Formats::ResizeFilter filter);

// SFF v2 flag. It sits on the pixels, so links share it.
OFS_CORE_EXPORT void setDecompressOnLoad(SpriteDocument& doc, int index, bool decompress);
OFS_CORE_EXPORT bool decompressesOnLoad(const SpriteDocument& doc, int index);

// Versions before 1.1 cannot store true-color sprites.
OFS_CORE_EXPORT bool hasTrueColor(const SpriteDocument& doc);
OFS_CORE_EXPORT int convertToIndexed(SpriteDocument& doc);

OFS_CORE_EXPORT QList<int> moveRows(SpriteDocument& doc, QList<int> rows, int delta, bool palettes = false);
OFS_CORE_EXPORT void arrange(SpriteDocument& doc, bool palettes = false);

OFS_CORE_EXPORT IssueList identifierIssues(const SpriteDocument& doc);

// Splits into " R", " G" and " B" sprites, each with a black to full color ramp palette.
OFS_CORE_EXPORT int splitChannels(SpriteDocument& doc, int index);

} // namespace OFS::SpriteEdits
