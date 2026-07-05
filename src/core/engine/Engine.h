#pragma once

#include "core/CoreExport.h"

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QtPlugin>

namespace OFS {

struct EngineVersion
{
    QString id;
    QString name;
};

struct ProjectTypeInfo
{
    QString id; // "char", "stage", "font", "storyboard", "motif", "fight" or "select"
    QString name;
};

// A game engine supported by the editor. Each one is a plugin under engines/<name>/ with its own
// data folder. The Engine plugins document in docs explains how plugins work.
class OFS_CORE_EXPORT Engine : public QObject
{
    Q_OBJECT

public:
    explicit Engine(QObject* parent = nullptr);
    ~Engine() override;

    // Short id, also the name of the data folder (data/engines/<name>).
    virtual QString name() const = 0;
    virtual QString displayName() const = 0;
    virtual QList<EngineVersion> versions() const = 0;
    virtual QList<ProjectTypeInfo> projectTypes() const = 0;

    virtual QStringList projectExtensions() const = 0;

    QString dataDir() const;
    QString userDataDir() const;
};

class EnginePlugin
{
public:
    virtual ~EnginePlugin() = default;
    virtual Engine* createEngine(QObject* parent) = 0;
};

} // namespace OFS

#define OFS_ENGINE_PLUGIN_IID "org.openfactorystudio.EnginePlugin/1"
Q_DECLARE_INTERFACE(OFS::EnginePlugin, OFS_ENGINE_PLUGIN_IID)
