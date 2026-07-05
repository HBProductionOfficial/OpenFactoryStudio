#include "engines/mugen/MugenEngine.h"

namespace OFS::Mugen {

MugenEngine::MugenEngine(QObject* parent)
    : Engine(parent)
{}

QString MugenEngine::name() const
{
    return QStringLiteral("mugen");
}

QString MugenEngine::displayName() const
{
    return QStringLiteral("M.U.G.E.N");
}

QList<EngineVersion> MugenEngine::versions() const
{
    return {
        {QStringLiteral("dosbeta"), tr("DOS M.U.G.E.N beta")},
        {QStringLiteral("winbeta"), tr("Win/Linux M.U.G.E.N beta")},
        {QStringLiteral("1.0"), tr("M.U.G.E.N 1.0")},
        {QStringLiteral("1.1"), tr("M.U.G.E.N 1.1")},
        {QStringLiteral("ext"), tr("MUGENext")},
    };
}

QList<ProjectTypeInfo> MugenEngine::projectTypes() const
{
    return {
        {QStringLiteral("char"), tr("Character")},
        {QStringLiteral("stage"), tr("Stage")},
        {QStringLiteral("font"), tr("Font")},
        {QStringLiteral("storyboard"), tr("Storyboard")},
        {QStringLiteral("motif"), tr("System screens")},
        {QStringLiteral("fight"), tr("Fight HUD & effects")},
        {QStringLiteral("select"), tr("Select screen")},
    };
}

QStringList MugenEngine::projectExtensions() const
{
    return {QStringLiteral("def")};
}

} // namespace OFS::Mugen
