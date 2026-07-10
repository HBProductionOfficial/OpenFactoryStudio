#pragma once

#include "core/CoreExport.h"

#include <QList>
#include <QString>

namespace OFS {

class Config;

struct OFS_CORE_EXPORT RunConfiguration
{
    QString name;
    QString executable;
    QString icon;
    QString parameters; // %0 = name of the project's run copy

    bool operator==(const RunConfiguration&) const = default;

    static QList<RunConfiguration> load(const Config& config);
    static void save(Config& config, const QList<RunConfiguration>& list);
};

} // namespace OFS
