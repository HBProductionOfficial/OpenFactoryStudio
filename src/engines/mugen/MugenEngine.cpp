#include "engines/mugen/MugenEngine.h"

#include "core/anims/AirDocument.h"
#include "core/app/AppContext.h"
#include "core/code/CompletionModels.h"
#include "core/code/VariableUsage.h"
#include "core/project/Project.h"
#include "core/project/ProjectFile.h"
#include "core/sprites/SpriteFile.h"
#include "core/syntax/SyntaxDatabase.h"
#include "engines/mugen/AirEdits.h"
#include "engines/mugen/MugenActions.h"
#include "engines/mugen/MugenControls.h"
#include "engines/mugen/MugenMenus.h"
#include "engines/mugen/MugenOutline.h"
#include "engines/mugen/MugenProject.h"
#include "engines/mugen/MugenRun.h"
#include "engines/mugen/MugenSyntax.h"
#include "engines/mugen/MugenVariables.h"
#include "engines/mugen/dialogs/OffsetViewerDialog.h"
#include "engines/mugen/dialogs/PalFXDialog.h"
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
    auto editor = [](const QString& id, const QString& title, const char* switchAction, WorkspaceType workspace,
                     const char* extraPanel, const char* prefix, const QString& filter, const char* suffix) {
        EditorInfo info;
        info.id = id;
        info.title = title;
        info.switchAction = QString::fromLatin1(switchAction);
        info.workspace = workspace;
        info.extraPanel = QString::fromLatin1(extraPanel);
        info.actionPrefix = QString::fromLatin1(prefix);
        info.fileFilter = filter;
        info.defaultSuffix = QString::fromLatin1(suffix);
        return info;
    };
    const QString all = tr("All files (*)");
    return {
        editor(QStringLiteral("definitions"), tr("Definitions"), "editor_def", WorkspaceType::Code, "", "def",
               tr("Definitions (*.def)") + QStringLiteral(";;") + all, "def"),
        editor(QStringLiteral("sprites"), tr("Sprites"), "editor_spr", WorkspaceType::Graphics, "palettes", "spr",
               tr("Sprites (*.sff)"), "sff"),
        editor(QStringLiteral("animations"), tr("Animations"), "editor_anim", WorkspaceType::Graphics, "thumbnails",
               "anim", tr("Animations (*.air)") + QStringLiteral(";;") + all, "air"),
        editor(QStringLiteral("backgrounds"), tr("Backgrounds"), "editor_bg", WorkspaceType::Graphics, "controllers",
               "", {}, ""),
        editor(QStringLiteral("commands"), tr("Commands"), "editor_cmd", WorkspaceType::Code, "", "cmd",
               tr("Commands (*.cmd)") + QStringLiteral(";;") + all, "cmd"),
        editor(QStringLiteral("states"), tr("States"), "editor_st", WorkspaceType::Code, "", "st",
               tr("States (*.cns *.st)") + QStringLiteral(";;") + all, "cns"),
        editor(QStringLiteral("sounds"), tr("Sounds"), "editor_snd", WorkspaceType::Sound, "", "snd",
               tr("Sounds (*.snd)"), "snd"),
        editor(QStringLiteral("emulator"), tr("Emulator"), "editor_emul", WorkspaceType::Emulator, "", "", {}, ""),
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

Project* MugenEngine::newProject(const QString& typeId, const QString& folder, QString* error)
{
    // Blank documents live in <data>/new/<type>/.
    const QString blank = dataDir() + QStringLiteral("/new/") + typeId;
    const QStringList definitions = QDir(blank).entryList({QStringLiteral("*.def")}, QDir::Files, QDir::Name);
    if (typeId.isEmpty() || definitions.isEmpty())
        return Engine::newProject(typeId, folder, error);
    auto* project = new MugenProject(this, QDir(folder).filePath(definitions.first()));
    if (!project->createNew(typeId, blank, error)) {
        delete project;
        return nullptr;
    }
    return project;
}

QMap<QString, QVariantMap> MugenEngine::scriptEnums() const
{
    return {
        {QStringLiteral("ProjectType"),
         {
             {QStringLiteral("Character"), QStringLiteral("char")},
             {QStringLiteral("Stage"), QStringLiteral("stage")},
             {QStringLiteral("Font"), QStringLiteral("font")},
             {QStringLiteral("Storyboard"), QStringLiteral("storyboard")},
             {QStringLiteral("Motif"), QStringLiteral("motif")},
             {QStringLiteral("FightFX"), QStringLiteral("fight")},
             {QStringLiteral("Select"), QStringLiteral("select")},
         }},
    };
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

void MugenEngine::scaleContactBoxes(Project& project, const QList<QPair<int, int>>& sprites, double widthScale,
                                    double heightScale)
{
    const QString air = project.editorFiles(QStringLiteral("animations")).value(0);
    if (air.isEmpty() || (!project.file(air) && !QFileInfo::exists(air)))
        return;
    if (CodeFile* file = project.codeFile(air, QStringLiteral("animations")))
        AirEdits::scaleContactBoxes(file->document(), sprites, widthScale, heightScale);
}

QStringList MugenEngine::toolActions() const
{
    return {QStringLiteral("st_throw"), QStringLiteral("st_offset"), QStringLiteral("st_palfx")};
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
    } else if (actionId == QLatin1String("st_offset")) {
        OffsetViewerDialog dialog(context.project, context.projects, context.parent);
        dialog.exec();
    } else if (actionId == QLatin1String("st_palfx")) {
        const QStringList sprites = context.project->editorFiles(QStringLiteral("sprites"));
        SpriteFile* spriteFile = sprites.isEmpty() || sprites.first().isEmpty()
                                     ? nullptr
                                     : context.project->spriteFile(sprites.first(), QStringLiteral("sprites"));
        PalFXDialog dialog(spriteFile, context.spriteIndex, dataDir() + QStringLiteral("/palFX"), context.parent);
        dialog.setInsertHandler([context](const QString& text) {
            if (context.insertInEditor)
                context.insertInEditor(QStringLiteral("states"), text);
            else if (context.insertText)
                context.insertText(text);
        });
        dialog.exec();
    }
}

RunCopy MugenEngine::prepareRun(const Project& project, const QString& engineDir) const
{
    return prepareRunCopy(project, engineDir);
}

QString MugenEngine::runLogFile(const QString& engineDir) const
{
    // Ikemen GO (run like M.U.G.E.N) writes its logs to save/logs, M.U.G.E.N to mugen.log.
    const QDir dir(engineDir);
    if (!dir.entryList({QStringLiteral("Ikemen_GO*")}, QDir::Files).isEmpty())
        return dir.filePath(QStringLiteral("save/logs"));
    return dir.filePath(QStringLiteral("mugen.log"));
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
    if (editorId == QLatin1String("animations")) {
        IssueList issues;
        const AirDocument doc = AirDocument::parse(text);
        for (const AirDocument::Problem& problem : doc.problems()) {
            Issue issue;
            issue.line = problem.line;
            issue.message = problem.message;
            issues << issue;
        }
        QHash<int, int> first;
        for (const AirAnimation& animation : doc.animations()) {
            const auto seen = first.constFind(animation.number);
            if (seen == first.cend()) {
                first.insert(animation.number, animation.headerLine);
                continue;
            }
            Issue issue;
            issue.severity = Issue::Severity::Warning;
            issue.line = animation.headerLine;
            issue.message = tr("Animation %1 is already defined at line %2").arg(animation.number).arg(*seen + 1);
            issues << issue;
        }
        return issues;
    }
    const QString fileType = syntaxFileType(project, editorId);
    const SyntaxDatabase* db = syntax();
    if (fileType.isEmpty() || !db || db->isEmpty())
        return {};
    return validateMugenCode(*db, fileType, project.isVersionDeclared() ? project.versionId() : QString(), text);
}

} // namespace OFS::Mugen
