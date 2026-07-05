#pragma once

#include "core/CoreExport.h"

#include <QList>
#include <QObject>
#include <QStringList>

namespace OFS {

class Engine;

class OFS_CORE_EXPORT EngineManager : public QObject
{
    Q_OBJECT

public:
    // Plugins built for another "apiVersion" in their JSON metadata are skipped.
    static constexpr int ApiVersion = 1;

    explicit EngineManager(QObject* parent = nullptr);
    ~EngineManager() override;

    void loadPlugins(const QStringList& searchDirs);
    void loadPlugins();

    void addEngine(Engine* engine);

    const QList<Engine*>& engines() const { return m_engines; }
    Engine* engine(const QString& name) const;

    const QStringList& errors() const { return m_errors; }

private:
    void loadPlugin(const QString& path);

    QList<Engine*> m_engines;
    QStringList m_errors;
};

} // namespace OFS
