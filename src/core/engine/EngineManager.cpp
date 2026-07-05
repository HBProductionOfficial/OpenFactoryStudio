#include "core/engine/EngineManager.h"

#include "core/app/Paths.h"
#include "core/engine/Engine.h"

#include <QDir>
#include <QJsonObject>
#include <QLibrary>
#include <QPluginLoader>

namespace OFS {

EngineManager::EngineManager(QObject* parent)
    : QObject(parent)
{}

EngineManager::~EngineManager() = default;

void EngineManager::loadPlugins()
{
    loadPlugins(Paths::engineSearchDirs());
}

void EngineManager::loadPlugins(const QStringList& searchDirs)
{
    for (const QString& root : searchDirs) {
        const QDir rootDir(root);
        if (!rootDir.exists())
            continue;
        const QStringList engineDirs = rootDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QString& sub : engineDirs) {
            const QDir dir(rootDir.filePath(sub));
            const QStringList files = dir.entryList(QDir::Files, QDir::Name);
            for (const QString& file : files) {
                const QString path = dir.filePath(file);
                if (QLibrary::isLibrary(path))
                    loadPlugin(path);
            }
        }
    }
}

void EngineManager::loadPlugin(const QString& path)
{
    QPluginLoader loader(path);
    const QJsonObject meta = loader.metaData();
    if (meta.isEmpty() || meta.value(QStringLiteral("IID")).toString() != QLatin1String(OFS_ENGINE_PLUGIN_IID))
        return; // not an engine plugin (e.g. a helper library shipped next to it)

    const int apiVersion =
        meta.value(QStringLiteral("MetaData")).toObject().value(QStringLiteral("apiVersion")).toInt();
    if (apiVersion != ApiVersion) {
        m_errors << tr("%1: built for engine API %2, expected %3").arg(path).arg(apiVersion).arg(ApiVersion);
        return;
    }

    QObject* instance = loader.instance();
    auto* plugin = qobject_cast<EnginePlugin*>(instance);
    if (!plugin) {
        m_errors << tr("%1: %2").arg(path, loader.errorString());
        return;
    }

    Engine* created = plugin->createEngine(this);
    if (!created) {
        m_errors << tr("%1: the plugin did not create an engine").arg(path);
        return;
    }
    if (engine(created->name())) {
        m_errors << tr("%1: an engine named '%2' is already loaded").arg(path, created->name());
        delete created;
        return;
    }
    m_engines << created;
}

void EngineManager::addEngine(Engine* engine)
{
    engine->setParent(this);
    m_engines << engine;
}

Engine* EngineManager::engine(const QString& name) const
{
    for (Engine* e : m_engines) {
        if (e->name().compare(name, Qt::CaseInsensitive) == 0)
            return e;
    }
    return nullptr;
}

} // namespace OFS
