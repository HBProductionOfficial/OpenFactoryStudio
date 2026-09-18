#pragma once

#include "core/CoreExport.h"

#include <QIcon>
#include <QList>
#include <QPair>
#include <QString>

namespace OFS {

class Config;

// An engine install used by Run in engine. In memory the list is numbered keys in the engine
// settings (run_count, run_0_name, run_0_exe, run_0_icon, run_0_params...). The settings file
// (mugen_config.xml) stores it the same way as Fighter Factory:
//     <configuration>
//         <engine name="Desktop MUGEN">
//             <param name="exe" value="C:\MUGEN\mugen.exe"/>
//             <param name="icon" value=""/>
//             <param name="params" value="%0 kfm"/>
//         </engine>
//     </configuration>
struct OFS_CORE_EXPORT RunConfiguration
{
    QString name;
    QString executable;
    QString icon;
    QString parameters; // %0 = name of the project's run copy

    bool operator==(const RunConfiguration&) const = default;

    QIcon displayIcon() const;

    static QList<RunConfiguration> load(const Config& config);
    static void save(Config& config, const QList<RunConfiguration>& list);

    static QList<RunConfiguration> readFile(const QString& path, bool* found = nullptr);
    static void loadEngineSettings(Config& config, const QString& defaultsPath, const QString& userPath);
    static bool writeEngineSettings(const Config& config, const QString& path, QString* error = nullptr);

    static QList<QPair<QString, QString>> parameterPresets();
};

} // namespace OFS
