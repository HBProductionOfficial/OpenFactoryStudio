#pragma once

#include "core/CoreExport.h"

#include <QList>
#include <QRegularExpression>
#include <QString>
#include <QStringList>

// View > Organizer: moves list items (sprites, animations, sounds, commands...) and their text.
namespace OFS::Organize {

OFS_CORE_EXPORT int deltaOf(const QString& actionId, int count);

// order[i] is the old position of the item now at i. *moved gets the rows' new positions.
OFS_CORE_EXPORT QList<int> moveOrder(int count, QList<int> rows, int delta, QList<int>* moved = nullptr);

// A section as the organizer moves it: from the comment lines right above its header to its last
// non-blank line (contentEnd, exclusive). Blank lines after it, up to `end`, stay in place.
struct Block
{
    int start = 0;
    int contentEnd = 0;
    int end = 0;
};

OFS_CORE_EXPORT QList<Block> sectionBlocks(const QStringList& lines, const QRegularExpression& header);

OFS_CORE_EXPORT void reorderBlocks(QStringList& lines, const QList<Block>& blocks, const QList<int>& order);

} // namespace OFS::Organize
