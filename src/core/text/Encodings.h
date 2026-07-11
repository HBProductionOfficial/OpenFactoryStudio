#pragma once

#include "core/CoreExport.h"

#include <QList>
#include <QString>

namespace OFS {

// Encodings of Edit > Encoding and the Default charset option, in menu order. Index 0 is the
// system encoding. The default_codepage setting stores the position, so never reorder entries.
struct OFS_CORE_EXPORT TextEncoding
{
    const char* group;
    QString names; // codec first, then aliases ("windows-1252, CP1252")

    QString groupName() const;
    QString codecName() const;
    QString displayText() const;

    static const QList<TextEncoding>& all();
    static int indexOf(const QString& name);
};

} // namespace OFS
