#pragma once

#include "core/CoreExport.h"

#include <QHash>
#include <QObject>

namespace OFS {

class ActionRegistry;
class Config;
class ControlRegistry;
class EngineManager;
class History;
class TaskList;
class ShortcutSettings;
class ThemeManager;

class OFS_CORE_EXPORT AppContext : public QObject
{
    Q_OBJECT

public:
    explicit AppContext(QObject* parent = nullptr);
    ~AppContext() override;

    static AppContext* instance();

    // Loads config, shortcuts and history. Engine plugins load in initializeEngines().
    void loadSettings();
    static double interfaceScaleFactor(const QString& configPath);
    bool saveSettings(QString* error = nullptr) const;
    void applySettings();

    void initializeEngines(const QStringList& engineSearchDirs = {});

    Config& config() const { return *m_config; }
    ShortcutSettings& shortcuts() const { return *m_shortcuts; }
    History& history() const { return *m_history; }
    TaskList& tasks() const { return *m_tasks; }
    ActionRegistry& actions() const { return *m_actions; }
    ControlRegistry& controls() const { return *m_controls; }
    EngineManager& engines() const { return *m_engines; }
    ThemeManager& theme() const { return *m_theme; }

    // <userDir>/<engine>_config.xml over <dataDir>/engines/<engine>/config.xml, loaded on first use.
    Config& engineConfig(const QString& engine) const;

private:
    Config* m_config;
    ShortcutSettings* m_shortcuts;
    History* m_history;
    TaskList* m_tasks;
    ActionRegistry* m_actions;
    ControlRegistry* m_controls;
    EngineManager* m_engines;
    ThemeManager* m_theme;
    mutable QHash<QString, Config*> m_engineConfigs;
};

} // namespace OFS
