#pragma once

#include "core/CoreExport.h"

#include <QObject>

namespace OFS {

class ActionRegistry;
class Config;
class ControlRegistry;
class EngineManager;
class History;
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
    bool saveSettings(QString* error = nullptr) const;

    Config& config() const { return *m_config; }
    ShortcutSettings& shortcuts() const { return *m_shortcuts; }
    History& history() const { return *m_history; }
    ActionRegistry& actions() const { return *m_actions; }
    ControlRegistry& controls() const { return *m_controls; }
    EngineManager& engines() const { return *m_engines; }
    ThemeManager& theme() const { return *m_theme; }

private:
    Config* m_config;
    ShortcutSettings* m_shortcuts;
    History* m_history;
    ActionRegistry* m_actions;
    ControlRegistry* m_controls;
    EngineManager* m_engines;
    ThemeManager* m_theme;
};

} // namespace OFS
