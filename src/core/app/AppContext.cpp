#include "core/app/AppContext.h"

#include "core/actions/ActionCatalog.h"
#include "core/actions/ActionRegistry.h"
#include "core/app/Paths.h"
#include "core/engine/Engine.h"
#include "core/engine/EngineManager.h"
#include "core/preset/ControlRegistry.h"
#include "core/settings/Config.h"
#include "core/settings/History.h"
#include "core/settings/ShortcutSettings.h"
#include "core/theme/ThemeManager.h"

namespace OFS {

namespace {
AppContext* g_instance = nullptr;
}

AppContext::AppContext(QObject* parent)
    : QObject(parent)
    , m_config(new Config(this))
    , m_shortcuts(new ShortcutSettings(this))
    , m_history(new History(this))
    , m_actions(new ActionRegistry(this))
    , m_controls(new ControlRegistry(m_actions))
    , m_engines(new EngineManager(this))
    , m_theme(new ThemeManager(this))
{
    Q_ASSERT_X(!g_instance, "AppContext", "only one AppContext may exist at a time");
    g_instance = this;
}

AppContext::~AppContext()
{
    delete m_controls;
    if (g_instance == this)
        g_instance = nullptr;
}

AppContext* AppContext::instance()
{
    return g_instance;
}

void AppContext::loadSettings()
{
    const QString data = Paths::dataDir();
    const QString user = Paths::userDir();
    m_config->load(data + QStringLiteral("/config.xml"), user + QStringLiteral("/config.xml"));
    m_shortcuts->load(data + QStringLiteral("/shortcuts.xml"), user + QStringLiteral("/shortcuts.xml"));
    m_history->setMaxEntries(m_config->intValue(QStringLiteral("max_history"), 10));
    m_history->load(user + QStringLiteral("/history.xml"));
    m_theme->setSearchDirs({user, data});
}

void AppContext::initializeEngines(const QStringList& engineSearchDirs)
{
    registerApplicationActions(*m_actions);
    if (engineSearchDirs.isEmpty())
        m_engines->loadPlugins();
    else
        m_engines->loadPlugins(engineSearchDirs);
    QStringList dataDirs;
    for (Engine* engine : m_engines->engines()) {
        engine->initialize(*this);
        dataDirs << engine->dataDir();
    }
    m_theme->setEngineDataDirs(dataDirs);
    m_actions->applyShortcuts(*m_shortcuts);
}

bool AppContext::saveSettings(QString* error) const
{
    bool ok = m_config->save(error);
    ok = m_shortcuts->save(ok ? error : nullptr) && ok;
    ok = m_history->save(ok ? error : nullptr) && ok;
    return ok;
}

} // namespace OFS
