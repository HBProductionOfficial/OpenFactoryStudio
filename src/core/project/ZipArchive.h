#pragma once

#include "core/CoreExport.h"

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

namespace OFS::Zip {

// Entry names use '/', and backslashes in names count as separators too. Names are UTF-8 when
// the archive marks them so, else the local code page.

struct OFS_CORE_EXPORT Entry
{
    QString name;
    qint64 size = 0;
    bool directory = false;
};

OFS_CORE_EXPORT QList<Entry> entries(const QString& archive, QString* error = nullptr);
// Refuses entries that would land outside folder.
OFS_CORE_EXPORT bool extract(const QString& archive, const QString& folder, QString* error = nullptr);
OFS_CORE_EXPORT QByteArray read(const QString& archive, const QString& name, QString* error = nullptr);
OFS_CORE_EXPORT bool create(const QString& archive, const QString& root, const QStringList& files,
                            QString* error = nullptr);

} // namespace OFS::Zip
