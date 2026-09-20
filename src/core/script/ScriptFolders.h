#pragma once

#include "core/CoreExport.h"

#include <QList>
#include <QString>
#include <QStringList>

namespace OFS::Script {

enum class ScriptType
{
    Common, // run by the program or other scripts (the throw creator's)
    Project,
    CodeSnippet, // listed by FFS without a use
    Tool,        // Tools menu items, main() runs
};

OFS_CORE_EXPORT QString scriptTypeFolder(ScriptType type);

// <dataDir>/scripts/<type> when engine is empty, else <dataDir>/engines/<engine>/scripts/<type>.
OFS_CORE_EXPORT QString scriptFolder(const QString& dataDir, const QString& engine, ScriptType type);
OFS_CORE_EXPORT QString scriptFolder(const QString& folder, ScriptType type);
OFS_CORE_EXPORT QString scriptFile(const QString& folder, ScriptType type, const QString& name);

// Scripts under a folder and its sub-folders: relative '/' paths without ".lua", sorted.
OFS_CORE_EXPORT QStringList listScripts(const QString& folder);

struct ToolScript
{
    QString caption;
    QString path;
};

// The shared Tools menu scripts first, then each engine's, by name.
OFS_CORE_EXPORT QList<ToolScript> toolScripts(const QString& dataDir, const QStringList& engines);

} // namespace OFS::Script
