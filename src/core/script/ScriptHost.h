#pragma once

#include "core/CoreExport.h"

#include <QString>

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
};

OFS_CORE_EXPORT Project* runProjectTemplate(const QString& path, const Environment& environment,
                                            QString* error = nullptr);

} // namespace Script
} // namespace OFS
