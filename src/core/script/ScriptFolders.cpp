#include "core/script/ScriptFolders.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>

#include <algorithm>

namespace OFS::Script {

QString scriptTypeFolder(ScriptType type)
{
    switch (type) {
    case ScriptType::Common:
        return QStringLiteral("common");
    case ScriptType::Project:
        return QStringLiteral("project");
    case ScriptType::CodeSnippet:
        return QStringLiteral("snippet");
    case ScriptType::Tool:
        return QStringLiteral("tool");
    }
    return QString();
}

QString scriptFolder(const QString& dataDir, const QString& engine, ScriptType type)
{
    const QString base = engine.isEmpty() ? dataDir : dataDir + QStringLiteral("/engines/") + engine;
    return QDir::cleanPath(base + QStringLiteral("/scripts/") + scriptTypeFolder(type));
}

QStringList listScripts(const QString& folder)
{
    QStringList scripts;
    const QDir root(folder);
    QDirIterator it(folder, {QStringLiteral("*.lua")}, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString relative = root.relativeFilePath(it.next());
        scripts << relative.left(relative.size() - 4);
    }
    std::sort(scripts.begin(), scripts.end(),
              [](const QString& a, const QString& b) { return a.compare(b, Qt::CaseInsensitive) < 0; });
    return scripts;
}

QList<ToolScript> toolScripts(const QString& dataDir, const QStringList& engines)
{
    QList<ToolScript> tools;
    QStringList folders{scriptFolder(dataDir, QString(), ScriptType::Tool)};
    for (const QString& engine : engines)
        folders << scriptFolder(dataDir, engine, ScriptType::Tool);
    for (const QString& folder : std::as_const(folders)) {
        const QFileInfoList files =
            QDir(folder).entryInfoList({QStringLiteral("*.lua")}, QDir::Files, QDir::Name | QDir::IgnoreCase);
        for (const QFileInfo& file : files)
            tools << ToolScript{file.completeBaseName(), file.absoluteFilePath()};
    }
    return tools;
}

} // namespace OFS::Script
