#include "engines/mugen/MugenEngine.h"

#include "core/app/AppContext.h"
#include "core/code/CompletionModels.h"
#include "core/code/VariableUsage.h"
#include "core/project/Project.h"
#include "core/syntax/SyntaxDatabase.h"
#include "engines/mugen/MugenActions.h"
#include "engines/mugen/MugenControls.h"
#include "engines/mugen/MugenMenus.h"
#include "engines/mugen/MugenOutline.h"
#include "engines/mugen/MugenProject.h"
#include "engines/mugen/MugenRun.h"
#include "engines/mugen/MugenSyntax.h"
#include "engines/mugen/MugenVariables.h"
#include "engines/mugen/dialogs/ThrowCreatorDialog.h"

#include <QDir>
#include <QFileInfo>

namespace OFS::Mugen {

MugenEngine::MugenEngine(QObject* parent)
    : Engine(parent)
{}

MugenEngine::~MugenEngine() = default;

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

const SyntaxDatabase* MugenEngine::syntax() const
{
    if (!m_syntax) {
        m_syntax = std::make_unique<SyntaxDatabase>();
        QString error;
        const QString path = dataDir() + QStringLiteral("/syntax.xml");
        if (!m_syntax->load(path, &error))
            qWarning("could not load %s: %s", qPrintable(path), qPrintable(error));
    }
    return m_syntax.get();
}

const CompletionModels* MugenEngine::completion() const
{
    if (!m_completion) {
        m_completion = std::make_unique<CompletionModels>();
        QString error;
        const QString path = dataDir() + QStringLiteral("/completer.xml");
        if (QFileInfo::exists(path) && !m_completion->load(path, &error))
            qWarning("could not load %s: %s", qPrintable(path), qPrintable(error));
    }
    return m_completion.get();
}

QList<OutlineNode> MugenEngine::outline(const Project& project, const QString& editorId, const QString& text) const
{
    return mugenOutline(text, syntax(), syntaxFileType(project, editorId));
}

VariableUsage MugenEngine::variableUsage(const Project& project, const QString& editorId, const QString& text) const
{
    Q_UNUSED(project);
    if (editorId != QLatin1String("states"))
        return {};
    return scanVariables(text);
}

QStringList MugenEngine::toolActions() const
{
    return {QStringLiteral("st_throw")};
}

void MugenEngine::runTool(const QString& actionId, const ToolContext& context)
{
    if (!context.project || context.project->engine() != this)
        return;
    if (actionId == QLatin1String("st_throw")) {
        ThrowCreatorDialog dialog(context.project, context.projects, dataDir() + QStringLiteral("/scripts/throw.lua"),
                                  context.parent);
        dialog.setInsertHandler(context.insertText);
        dialog.setIcons(context.icon);
        dialog.exec();
    }
}

RunCopy MugenEngine::prepareRun(const Project& project, const QString& engineDir) const
{
    return prepareRunCopy(project, engineDir);
}

QString MugenEngine::runLogFile(const QString& engineDir) const
{
    return QDir(engineDir).filePath(QStringLiteral("mugen.log"));
}

QStringList MugenEngine::codeTemplateDirs(const QString& editorId) const
{
    if (editorId != QLatin1String("states"))
        return {};
    return {dataDir() + QStringLiteral("/sctrls"), userDataDir() + QStringLiteral("/sctrls")};
}

QStringList MugenEngine::documentationVariants(const Project& project) const
{
    // Version folders hold pages that differ from the common ones for that engine release.
    const QString version = project.versionId();
    if (version == QLatin1String("1.1") || version == QLatin1String("ext"))
        return {QStringLiteral("1.1"), QStringLiteral("1.0"), QStringLiteral("common")};
    if (version == QLatin1String("1.0"))
        return {QStringLiteral("1.0"), QStringLiteral("common")};
    if (version == QLatin1String("winbeta"))
        return {QStringLiteral("win"), QStringLiteral("common")};
    return {QStringLiteral("dos"), QStringLiteral("common")};
}

QString MugenEngine::syntaxFileType(const Project& project, const QString& editorId) const
{
    if (editorId == QLatin1String("commands"))
        return QStringLiteral("command");
    if (editorId == QLatin1String("states"))
        return QStringLiteral("state");
    if (editorId == QLatin1String("definitions")) {
        const QString type = project.typeId();
        if (type == QLatin1String("char") || type == QLatin1String("stage") || type == QLatin1String("motif") ||
            type == QLatin1String("fight") || type == QLatin1String("select") || type == QLatin1String("font") ||
            type == QLatin1String("storyboard"))
            return type;
    }
    return {};
}

IssueList MugenEngine::validate(const Project& project, const QString& editorId, const QString& text) const
{
    const QString fileType = syntaxFileType(project, editorId);
    const SyntaxDatabase* db = syntax();
    if (fileType.isEmpty() || !db || db->isEmpty())
        return {};
    return validateMugenCode(*db, fileType, project.isVersionDeclared() ? project.versionId() : QString(), text);
}

} // namespace OFS::Mugen
