#pragma once

// Internal FF API shared by project templates, the Script editor and the Tools menu.

#include "core/script/ScriptHost.h"

#include <QDir>
#include <QFileInfo>
#include <QObject>
#include <QPointer>
#include <QString>

#include <sol/sol.hpp>

#include <vector>

namespace OFS {
class Engine;
class Project;
} // namespace OFS

namespace OFS::Script {

inline std::string toStd(const QString& text)
{
    return text.toStdString();
}

inline QString fromStd(const std::string& text)
{
    return QString::fromStdString(text);
}

// What one script run made. Its widgets and unopened projects go away with it.
struct Session
{
    Environment environment;
    QString scriptPath;
    QString context; // translation context: the script's base name
    std::vector<QPointer<QObject>> objects;
    std::vector<QPointer<Project>> projects;

    template <typename T>
    T* track(T* object)
    {
        objects.emplace_back(object);
        return object;
    }

    ~Session();
};

Engine* findEngine(const Session& session, const QString& name);

struct ScriptRgb
{
    int r = 0;
    int g = 0;
    int b = 0;
};

// FF.Archive: a folder or a ZIP file.
struct ScriptArchive
{
    QString root;
    bool zip = false;
    bool readOnly = false;

    QString folder() const { return zip ? QFileInfo(root).absolutePath() : root; }
    QString onDisk(const QString& name) const
    {
        const QString path = QDir::fromNativeSeparators(name);
        return QDir::isAbsolutePath(path) ? QDir::cleanPath(path) : QDir::cleanPath(root + QLatin1Char('/') + path);
    }
    static QString entryName(const QString& name)
    {
        QString path = QDir::fromNativeSeparators(name);
        while (path.startsWith(QLatin1Char('/')))
            path.remove(0, 1);
        while (path.endsWith(QLatin1Char('/')))
            path.chop(1);
        return path;
    }
};

void registerApi(sol::state& lua, Session& session);
void registerProjectApi(sol::state& lua, sol::table api, Session& session);
void registerFileApi(sol::state& lua, sol::table api);

} // namespace OFS::Script
