#pragma once

#include "core/CoreExport.h"

#include <QList>
#include <QString>

#include <functional>

class QWidget;

namespace OFS {

class EngineManager;
class Project;

namespace Script {

struct Environment
{
    EngineManager* engines = nullptr;
    QWidget* parent = nullptr;
    QString projectFolder;
    // Opens a project at once (Script editor, Tools menu). Empty for templates, which return it.
    std::function<void(Project*)> openProject;
    std::function<QList<Project*>()> projects;
    std::function<Project*()> currentProject;
    std::function<void(const QString&)> output;
    std::function<bool()> stopRequested;
};

struct RunResult
{
    bool ok = true;
    bool stopped = false;
    QString error;
};

// Runs a script in a fresh Lua state, then its global main() if it has one.
OFS_CORE_EXPORT RunResult runScript(const QString& code, const QString& source, const Environment& environment);
OFS_CORE_EXPORT RunResult runScriptFile(const QString& path, const Environment& environment);

// Project > New: runs a template script that returns a project, or nil to cancel.
OFS_CORE_EXPORT Project* runProjectTemplate(const QString& path, const Environment& environment,
                                            QString* error = nullptr);

} // namespace Script
} // namespace OFS
