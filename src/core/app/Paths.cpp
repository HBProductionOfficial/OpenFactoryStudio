#include "core/app/Paths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

#include <optional>

namespace OFS {

namespace {

std::optional<QString> g_dataDir;
std::optional<QString> g_userDir;

QString appDir()
{
    return QCoreApplication::applicationDirPath();
}

QString clean(const QString& path)
{
    return QDir::cleanPath(QDir(path).absolutePath());
}

} // namespace

QString Paths::dataDir()
{
    if (g_dataDir)
        return *g_dataDir;

    const QString env = qEnvironmentVariable("OFS_DATA_DIR");
    QStringList candidates;
    if (!env.isEmpty())
        candidates << env;
    candidates << appDir() + QStringLiteral("/data") << appDir() + QStringLiteral("/../share/openfactorystudio")
               << appDir() + QStringLiteral("/../Resources/data");

    for (const QString& candidate : candidates) {
        if (QFileInfo(candidate).isDir()) {
            g_dataDir = clean(candidate);
            return *g_dataDir;
        }
    }
    g_dataDir = clean(appDir() + QStringLiteral("/data"));
    return *g_dataDir;
}

QString Paths::userDir()
{
    if (g_userDir)
        return *g_userDir;

    QString dir = qEnvironmentVariable("OFS_CONFIG_DIR");
    if (dir.isEmpty()) {
        dir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/") +
              QStringLiteral("Open Factory Studio");
    }
    QDir().mkpath(dir);
    g_userDir = clean(dir);
    return *g_userDir;
}

QString Paths::engineDataDir(const QString& engine)
{
    return dataDir() + QStringLiteral("/engines/") + engine;
}

QString Paths::userEngineDir(const QString& engine)
{
    return userDir() + QStringLiteral("/engines/") + engine;
}

QStringList Paths::engineSearchDirs()
{
    QStringList dirs;
    dirs << clean(appDir() + QStringLiteral("/engines"));
    dirs << clean(appDir() + QStringLiteral("/../lib/openfactorystudio/engines"));
    dirs << clean(appDir() + QStringLiteral("/../PlugIns/engines"));
    dirs.removeDuplicates();
    return dirs;
}

QStringList Paths::extensionSearchDirs()
{
    QStringList dirs;
    dirs << clean(appDir() + QStringLiteral("/extensions"));
    dirs << clean(appDir() + QStringLiteral("/../lib/openfactorystudio/extensions"));
    dirs << clean(appDir() + QStringLiteral("/../PlugIns/extensions"));
    dirs.removeDuplicates();
    return dirs;
}

QString Paths::locate(const QString& relative)
{
    const QString user = userDir() + QLatin1Char('/') + relative;
    if (QFileInfo::exists(user))
        return user;
    const QString data = dataDir() + QLatin1Char('/') + relative;
    if (QFileInfo::exists(data))
        return data;
    return {};
}

void Paths::reset()
{
    g_dataDir.reset();
    g_userDir.reset();
}

} // namespace OFS
