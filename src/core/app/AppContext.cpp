#include "core/app/AppContext.h"

#include "core/actions/ActionCatalog.h"
#include "core/actions/ActionRegistry.h"
#include "core/app/Paths.h"
#include "core/engine/Engine.h"
#include "core/engine/EngineManager.h"
#include "core/preset/ControlRegistry.h"
#include "core/project/ProjectFile.h"
#include "core/settings/Config.h"
#include "core/settings/History.h"
#include "core/settings/ShortcutSettings.h"
#include "core/text/Encodings.h"
#include "core/theme/ThemeManager.h"

namespace OFS {

namespace {

AppContext* g_instance = nullptr;

QString engineConfigDefaultsPath(const QString& engine)
{
    return Paths::engineDataDir(engine) + QStringLiteral("/config.xml");
}

QString engineConfigUserPath(const QString& engine)
{
    return Paths::userDir() + QLatin1Char('/') + engine + QStringLiteral("_config.xml");
}

} // namespace

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
    for (auto it = m_engineConfigs.cbegin(); it != m_engineConfigs.cend(); ++it)
        it.value()->load(engineConfigDefaultsPath(it.key()), engineConfigUserPath(it.key()));
    applySettings();
}

void AppContext::applySettings()
{
    const QList<TextEncoding>& encodings = TextEncoding::all();
    const int codePage = m_config->intValue(QStringLiteral("default_codepage"), 0);
    CodeFile::setFallbackEncoding(codePage > 0 && codePage < int(encodings.size()) ? encodings[codePage].codecName()
                                                                                   : QStringLiteral("System"));
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
    for (const Config* engineConfig : std::as_const(m_engineConfigs))
        ok = engineConfig->save(ok ? error : nullptr) && ok;
    return ok;
}

Config& AppContext::engineConfig(const QString& engine) const
{
    auto it = m_engineConfigs.constFind(engine);
    if (it != m_engineConfigs.cend())
        return *it.value();
    auto* config = new Config(const_cast<AppContext*>(this));
    config->load(engineConfigDefaultsPath(engine), engineConfigUserPath(engine));
    m_engineConfigs.insert(engine, config);
    return *config;
}

} // namespace OFS
