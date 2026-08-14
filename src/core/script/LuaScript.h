#pragma once

#include "core/CoreExport.h"

#include <QString>
#include <QVariant>

namespace OFS::Script {

// Calls a global function of a Lua script with `argument` converted to Lua. Engines use it for the
// text their tools generate, so the output can change without a rebuild.
OFS_CORE_EXPORT QString callFunction(const QString& path, const QString& function, const QVariant& argument,
                                     QString* error = nullptr);

} // namespace OFS::Script
