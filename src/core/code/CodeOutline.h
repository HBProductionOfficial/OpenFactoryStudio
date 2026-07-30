#pragma once

#include "core/CoreExport.h"

#include <QList>
#include <QString>

namespace OFS {

struct OFS_CORE_EXPORT OutlineNode
{
    QString text;
    QString icon;
    int line = -1;
    QList<OutlineNode> children;
};

using Outline = QList<OutlineNode>;

// The default outline: one entry per section header, with its parameter names (lower case) below.
OFS_CORE_EXPORT Outline sectionOutline(const QString& text);

} // namespace OFS
