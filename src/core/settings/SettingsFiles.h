#pragma once

#include <QString>

namespace OFS::SettingsFiles {

inline QString config(const QString& dir)
{
    return dir + QStringLiteral("/config.xml");
}
inline QString shortcuts(const QString& dir)
{
    return dir + QStringLiteral("/shortcuts.xml");
}
inline QString history(const QString& dir)
{
    return dir + QStringLiteral("/history.xml");
}
// mugen_config.xml in a user folder. The defaults are config.xml in the engine data folder.
inline QString engineConfig(const QString& dir, const QString& engine)
{
    return dir + u'/' + engine + QStringLiteral("_config.xml");
}

} // namespace OFS::SettingsFiles
