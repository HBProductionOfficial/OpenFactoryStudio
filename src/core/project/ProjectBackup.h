#pragma once

#include "core/CoreExport.h"

#include <QDateTime>
#include <QList>
#include <QString>

namespace OFS {

class Project;

// Project > Create backup and Load backup: ZIP archives of a project's files in the "backup"
// folder beside its .def, or beside the archive for an archive project. Names hold the time they
// were made: "04.10.2026_10.32.40_pm_.zip", or "..._Auto.zip" for backups made when a project
// opens.
namespace Backup {

struct Item
{
    QString path;
    QString label;
    QDateTime time;
    bool automatic = false;
};

OFS_CORE_EXPORT QString folder(const Project& project);
OFS_CORE_EXPORT QString fileName(const QDateTime& time, bool automatic);
OFS_CORE_EXPORT QDateTime timeOf(const QString& name, bool* automatic = nullptr);

OFS_CORE_EXPORT QString create(const Project& project, const QDateTime& time, bool automatic, QString* error = nullptr);
OFS_CORE_EXPORT QList<Item> list(const Project& project);
// Writes a backup's files over the project folder. Reload the project afterwards.
OFS_CORE_EXPORT bool restore(const Project& project, const QString& backupPath, QString* error = nullptr);

} // namespace Backup
} // namespace OFS
