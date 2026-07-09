#include "engines/mugen/MugenEngine.h"

#include "core/app/AppContext.h"
#include "engines/mugen/MugenActions.h"
#include "engines/mugen/MugenControls.h"
#include "engines/mugen/MugenMenus.h"
#include "engines/mugen/MugenProject.h"

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

void MugenEngine::initialize(AppContext& context)
{
    registerMugenActions(context.actions());
    registerMugenControls(context.controls());
}

QList<EditorInfo> MugenEngine::editors() const
{
    return {
        {QStringLiteral("definitions"), tr("Definitions"), QStringLiteral("editor_def"), WorkspaceType::Code, {}},
        {QStringLiteral("sprites"), tr("Sprites"), QStringLiteral("editor_spr"), WorkspaceType::Graphics,
         QStringLiteral("palettes")},
        {QStringLiteral("animations"), tr("Animations"), QStringLiteral("editor_anim"), WorkspaceType::Graphics,
         QStringLiteral("thumbnails")},
        {QStringLiteral("backgrounds"), tr("Backgrounds"), QStringLiteral("editor_bg"), WorkspaceType::Graphics,
         QStringLiteral("controllers")},
        {QStringLiteral("commands"), tr("Commands"), QStringLiteral("editor_cmd"), WorkspaceType::Code, {}},
        {QStringLiteral("states"), tr("States"), QStringLiteral("editor_st"), WorkspaceType::Code, {}},
        {QStringLiteral("sounds"), tr("Sounds"), QStringLiteral("editor_snd"), WorkspaceType::Sound, {}},
        {QStringLiteral("emulator"), tr("Emulator"), QStringLiteral("editor_emul"), WorkspaceType::Emulator, {}},
    };
}

QList<MenuSpec> MugenEngine::menus() const
{
    return mugenMenus();
}

QList<MenuItemSpec> MugenEngine::toolsMenuItems() const
{
    return mugenToolsMenuItems();
}

Project* MugenEngine::openProject(const QString& filePath, QString* error)
{
    auto* project = new MugenProject(this, filePath);
    if (!project->load(error)) {
        delete project;
        return nullptr;
    }
    return project;
}

} // namespace OFS::Mugen
