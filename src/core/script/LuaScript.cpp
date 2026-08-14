#include "core/script/LuaScript.h"

#include <QFile>

#include <sol/sol.hpp>

namespace OFS::Script {

namespace {

sol::object toLua(sol::state_view lua, const QVariant& value)
{
    switch (value.typeId()) {
    case QMetaType::UnknownType:
        return sol::make_object(lua, sol::lua_nil);
    case QMetaType::Bool:
        return sol::make_object(lua, value.toBool());
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::Long:
    case QMetaType::LongLong:
    case QMetaType::Short:
        return sol::make_object(lua, static_cast<lua_Integer>(value.toLongLong()));
    case QMetaType::Double:
    case QMetaType::Float:
        return sol::make_object(lua, value.toDouble());
    case QMetaType::QVariantMap: {
        sol::table table = lua.create_table();
        const QVariantMap map = value.toMap();
        for (auto it = map.cbegin(); it != map.cend(); ++it)
            table[it.key().toStdString()] = toLua(lua, it.value());
        return table;
    }
    case QMetaType::QVariantList:
    case QMetaType::QStringList: {
        sol::table table = lua.create_table();
        const QVariantList list = value.toList();
        for (qsizetype i = 0; i < list.size(); ++i)
            table[i + 1] = toLua(lua, list.at(i));
        return table;
    }
    default:
        return sol::make_object(lua, value.toString().toStdString());
    }
}

} // namespace

QString callFunction(const QString& path, const QString& function, const QVariant& argument, QString* error)
{
    auto fail = [error](const QString& message) {
        if (error)
            *error = message;
        return QString();
    };
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return fail(QStringLiteral("%1: %2").arg(path, file.errorString()));

    sol::state lua;
    lua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::table, sol::lib::math, sol::lib::utf8);
    const std::string code = file.readAll().toStdString();
    const std::string chunk = "@" + path.toStdString();
    sol::protected_function_result loaded = lua.safe_script(code, sol::script_pass_on_error, chunk);
    if (!loaded.valid()) {
        const sol::error err = loaded;
        return fail(QString::fromUtf8(err.what()));
    }
    sol::object entry = lua[function.toStdString()];
    if (entry.get_type() != sol::type::function)
        return fail(QStringLiteral("%1: no function '%2'").arg(path, function));
    sol::protected_function fn = entry;
    sol::protected_function_result result = fn(toLua(lua, argument));
    if (!result.valid()) {
        const sol::error err = result;
        return fail(QString::fromUtf8(err.what()));
    }
    const sol::object value = result;
    if (value.get_type() != sol::type::string && value.get_type() != sol::type::number)
        return fail(QStringLiteral("%1: '%2' did not return text").arg(path, function));
    if (value.get_type() == sol::type::string)
        return QString::fromStdString(value.as<std::string>());
    // A number keeps Lua's own formatting ("3", "1.5").
    sol::protected_function toString = lua["tostring"];
    const std::string text = toString(value);
    return QString::fromStdString(text);
}

} // namespace OFS::Script
