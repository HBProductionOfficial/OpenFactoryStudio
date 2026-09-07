#include "core/script/ScriptHost.h"

#include "core/project/Project.h"
#include "core/script/LuaApi.h"
#include "core/script/ScriptTranslation.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QTranslator>

namespace OFS::Script {

namespace {

constexpr int HookInstructions = 10000; // instructions between event checks
constexpr int EventInterval = 50;
const char* const UnknownError = "An unknown error has triggered the default error handler";

// The current run, for the instruction hook. Only one script runs at a time, on the UI thread.
struct Running
{
    const Environment* environment = nullptr;
    QElapsedTimer events;
    bool stopped = false;
};
thread_local Running* t_running = nullptr;

void hook(lua_State* L, lua_Debug*)
{
    Running* run = t_running;
    if (!run)
        return;
    if (run->events.elapsed() >= EventInterval) {
        run->events.restart();
        if (QCoreApplication::instance())
            QCoreApplication::processEvents();
    }
    if (run->environment->stopRequested && run->environment->stopRequested()) {
        run->stopped = true;
        luaL_error(L, "script stopped");
    }
}

int traceback(lua_State* L)
{
    const char* message = lua_tostring(L, 1);
    if (!message) {
        if (luaL_callmeta(L, 1, "__tostring") && lua_type(L, -1) == LUA_TSTRING)
            message = lua_tostring(L, -1);
        else
            message = UnknownError;
    }
    luaL_traceback(L, L, message, 1);
    return 1;
}

bool protectedCall(lua_State* L, QString* error)
{
    const int function = lua_gettop(L);
    lua_pushcfunction(L, traceback);
    lua_insert(L, function);
    const int status = lua_pcall(L, 0, 0, function);
    lua_remove(L, function);
    if (status == LUA_OK)
        return true;
    QString text = QString::fromUtf8(lua_tostring(L, -1));
    lua_pop(L, 1);
    // Drop the protected call's own frame, it tells nothing.
    static const QString tail = QStringLiteral("\n\t[C]: in ?");
    while (text.endsWith(tail))
        text.chop(tail.size());
    *error = QStringLiteral("sol: runtime error: ") + text;
    return false;
}

class RunTranslation
{
public:
    explicit RunTranslation(const QString& scriptPath)
    {
        if (!scriptPath.isEmpty() && QCoreApplication::instance() && loadScriptTranslation(m_translator, scriptPath))
            m_installed = QCoreApplication::installTranslator(&m_translator);
    }
    ~RunTranslation()
    {
        if (m_installed)
            QCoreApplication::removeTranslator(&m_translator);
    }
    RunTranslation(const RunTranslation&) = delete;
    RunTranslation& operator=(const RunTranslation&) = delete;

private:
    QTranslator m_translator;
    bool m_installed = false;
};

void prepare(sol::state& lua, Session& session)
{
    lua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::table, sol::lib::math, sol::lib::utf8);
    lua["base"] = lua.globals();
    registerApi(lua, session);
    if (!session.scriptPath.isEmpty())
        lua["ScriptPath"] = toStd(session.scriptPath);

    const Environment* environment = &session.environment;
    lua.set_function("debugOutput", [environment](sol::object value, sol::this_state state) {
        if (!environment->output)
            return;
        if (value.get_type() != sol::type::string && value.get_type() != sol::type::number)
            return;
        lua_State* L = state;
        value.push(L);
        environment->output(QString::fromUtf8(lua_tostring(L, -1)));
        lua_pop(L, 1);
    });
    lua.set_function("print", [environment](sol::variadic_args args, sol::this_state state) {
        if (!environment->output)
            return;
        lua_State* L = state;
        QStringList parts;
        for (const sol::stack_proxy& arg : args) {
            sol::stack::push(L, arg);
            parts << QString::fromUtf8(luaL_tolstring(L, -1, nullptr));
            lua_pop(L, 2);
        }
        environment->output(parts.join(QLatin1Char('\t')) + QLatin1Char('\n'));
    });
}

} // namespace

RunResult runScript(const QString& code, const QString& source, const Environment& environment)
{
    Session session;
    session.environment = environment;
    if (!source.isEmpty()) {
        session.scriptPath = QFileInfo(source).absolutePath();
        session.context = QFileInfo(source).completeBaseName();
    }
    RunTranslation translation(source);
    sol::state lua;
    prepare(lua, session);
    lua_State* L = lua.lua_state();

    Running running;
    running.environment = &session.environment;
    running.events.start();
    Running* outer = t_running;
    t_running = &running;
    lua_sethook(L, hook, LUA_MASKCOUNT, HookInstructions);

    RunResult result;
    const QByteArray bytes = code.toUtf8();
    const QByteArray name =
        source.isEmpty() ? bytes
                         : QByteArray("@") + QDir::toNativeSeparators(QFileInfo(source).absoluteFilePath()).toUtf8();
    if (luaL_loadbuffer(L, bytes.constData(), size_t(bytes.size()), name.constData()) != LUA_OK) {
        result.ok = false;
        result.error = QStringLiteral("sol: syntax error: ") + QString::fromUtf8(lua_tostring(L, -1));
        lua_pop(L, 1);
    } else if (!protectedCall(L, &result.error)) {
        result.ok = false;
    } else {
        lua_getglobal(L, "main");
        if (lua_isfunction(L, -1)) {
            if (!protectedCall(L, &result.error))
                result.ok = false;
        } else {
            lua_pop(L, 1);
        }
    }
    lua_sethook(L, nullptr, 0, 0);
    t_running = outer;
    result.stopped = running.stopped;
    return result;
}

RunResult runScriptFile(const QString& path, const Environment& environment)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        RunResult result;
        result.ok = false;
        result.error = QStringLiteral("%1: %2").arg(QDir::toNativeSeparators(path), file.errorString());
        return result;
    }
    return runScript(QString::fromUtf8(file.readAll()), path, environment);
}

Project* runProjectTemplate(const QString& path, const Environment& environment, QString* error)
{
    auto fail = [error](const QString& message) -> Project* {
        if (error)
            *error = message;
        return nullptr;
    };
    if (error)
        error->clear();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return fail(QStringLiteral("%1: %2").arg(QDir::toNativeSeparators(path), file.errorString()));

    Session session;
    session.environment = environment;
    session.environment.openProject = nullptr;
    session.scriptPath = QFileInfo(path).absolutePath();
    session.context = QFileInfo(path).completeBaseName();

    RunTranslation translation(path);
    sol::state lua;
    prepare(lua, session);

    const std::string code = file.readAll().toStdString();
    const std::string chunk = "@" + toStd(QDir::toNativeSeparators(path));
    sol::protected_function_result result = lua.safe_script(code, sol::script_pass_on_error, chunk);
    if (!result.valid()) {
        const sol::error err = result;
        return fail(QString::fromUtf8(err.what()));
    }
    const sol::object value = result;
    if (value.get_type() == sol::type::lua_nil || value.get_type() == sol::type::none)
        return nullptr;
    if (!value.is<Project*>())
        return fail(QStringLiteral("%1: the script did not return a project").arg(QDir::toNativeSeparators(path)));
    Project* project = value.as<Project*>();
    for (QPointer<Project>& owned : session.projects) {
        if (owned == project)
            owned.clear();
    }
    return project;
}

} // namespace OFS::Script
