#pragma once

#include "core/CoreExport.h"

#include <QByteArray>
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

    // name can also be "System" for the system code page.
    static QString decode(const QByteArray& data, const QString& name, bool* ok = nullptr);
    static QByteArray encode(const QString& text, const QString& name, bool* ok = nullptr);
    static bool isAvailable(const QString& name);
};

} // namespace OFS
