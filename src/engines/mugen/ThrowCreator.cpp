#include "engines/mugen/ThrowCreator.h"

#include "core/script/LuaScript.h"

#include <QVariantList>
#include <QVariantMap>

namespace OFS::Mugen {

QString throwStates(const QString& scriptPath, const ThrowSettings& settings, QString* error)
{
    QVariantList binds;
    for (const ThrowBind& bind : settings.binds)
        binds << QVariantMap{
            {QStringLiteral("x"), bind.x}, {QStringLiteral("y"), bind.y}, {QStringLiteral("priority"), bind.priority}};
    const QVariantMap arguments{
        {QStringLiteral("stateno"), settings.stateNo},
        {QStringLiteral("startAnim"), settings.startAnim},
        {QStringLiteral("throwAnim"), settings.throwAnim},
        {QStringLiteral("thrownAnim"), settings.thrownAnim},
        {QStringLiteral("xVel"), settings.xVelocity},
        {QStringLiteral("yVel"), settings.yVelocity},
        {QStringLiteral("binds"), binds},
    };
    return Script::callFunction(scriptPath, QStringLiteral("throw"), arguments, error);
}

} // namespace OFS::Mugen
