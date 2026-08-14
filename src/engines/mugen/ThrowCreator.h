#pragma once

#include <QList>
#include <QString>

namespace OFS::Mugen {

// Where the opponent is held on one element of the throw animation.
struct ThrowBind
{
    double x = 75.0;
    double y = 0.0;
    int priority = 0;
};

struct ThrowSettings
{
    int stateNo = 0; // StateDef of the attempt. The throw is stateNo + 10, the opponent + 20/21.
    int startAnim = 0;
    int throwAnim = 0;
    int thrownAnim = 0;
    int xVelocity = 0;
    int yVelocity = 0;
    QList<ThrowBind> binds; // one per element, up to the element the throw stops on
};

// Written by scripts/common/Throw.lua (function "throw"). Empty with *error set when the script fails.
QString throwStates(const QString& scriptPath, const ThrowSettings& settings, QString* error = nullptr);

} // namespace OFS::Mugen
