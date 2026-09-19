#include "engines/mugen/MugenEngine.h"
#include "core/sprites/QtImages.h"
#include "engines/mugen/emulator/EmulatorWidget.h"

#include "core/anims/AirDocument.h"
#include "core/app/AppContext.h"
#include "core/code/CompletionModels.h"
#include "core/code/VariableUsage.h"
#include "core/project/Project.h"
#include "core/project/ProjectFile.h"
#include "core/sprites/SpriteFile.h"
#include "core/syntax/SyntaxDatabase.h"
#include "engines/mugen/AirEdits.h"
#include "engines/mugen/DefFile.h"
#include "engines/mugen/MugenActions.h"
#include "engines/mugen/MugenControls.h"
#include "engines/mugen/MugenHtmlExport.h"
#include "engines/mugen/MugenMenus.h"
#include "engines/mugen/MugenOutline.h"
#include "engines/mugen/MugenProject.h"
#include "engines/mugen/MugenRun.h"
#include "engines/mugen/MugenSyntax.h"
#include "engines/mugen/MugenVariables.h"
#include "engines/mugen/dialogs/ExportHtmlDialog.h"
#include "engines/mugen/dialogs/OffsetViewerDialog.h"
#include "engines/mugen/dialogs/PalFXDialog.h"
#include "engines/mugen/dialogs/ThrowCreatorDialog.h"

#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QTextDocument>

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
    return QStringLiteral("MUGENext");
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

Project* MugenEngine::openSingleFile(const QString& filePath, QString* error)
{
    auto* project = new MugenProject(this, filePath);
    if (!project->loadSingle(error)) {
        delete project;
        return nullptr;
    }
    return project;
}

QString MugenEngine::singleFileFilter() const
{
    return QStringLiteral("%1 (*.def *.sff *.snd *.air *.cns *.st *.cmd *.*)").arg(displayName());
}

namespace {

// MUGEN.ProjectType, numbered as scripts know it.
const QStringList& scriptTypeIds()
{
    static const QStringList ids = {QStringLiteral("char"),  QStringLiteral("stage"), QStringLiteral("storyboard"),
                                    QStringLiteral("motif"), QStringLiteral("font"),  QStringLiteral("select"),
                                    QStringLiteral("fight")};
    return ids;
}

} // namespace

QMap<QString, QVariantMap> MugenEngine::scriptEnums() const
{
    return {
        {QStringLiteral("ProjectType"),
         {
             {QStringLiteral("Character"), 0},
             {QStringLiteral("Stage"), 1},
             {QStringLiteral("Storyboard"), 2},
             {QStringLiteral("Motif"), 3},
             {QStringLiteral("Font"), 4},
             {QStringLiteral("Select"), 5},
             {QStringLiteral("Fight"), 6},
         }},
    };
}

QString MugenEngine::scriptProjectTypeId(int value) const
{
    return scriptTypeIds().value(value);
}

int MugenEngine::scriptProjectType(const QString& typeId) const
{
    return int(scriptTypeIds().indexOf(typeId));
}

EngineWorkspace* MugenEngine::createWorkspace(const QString& editorId, Project& project, AppContext& context,
                                              QWidget* parent)
{
    if (editorId == QLatin1String("emulator"))
        return new EmulatorWidget(project, context, parent);
    return nullptr;
}

QMap<int, QString> MugenEngine::scriptFileFilters() const
{
    return {{0, QStringLiteral("*.def")}, {1, QStringLiteral("*.sff")},          {2, QStringLiteral("*.snd")},
            {3, QStringLiteral("*.air")}, {4, QStringLiteral("*.cns *.st *.*")}, {6, QStringLiteral("*.cmd *.*")}};
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
    return mugenOutline(text, syntax(), syntaxFileTypeForText(project, editorId, text));
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

void MugenEngine::exportHtml(const ToolContext& context)
{
    Project* project = context.project;
    const QString kind = htmlLayoutKind(project->typeId());
    const QList<QPair<QString, QString>> layouts = htmlLayouts({dataDir(), userDataDir()}, kind);
    if (kind.isEmpty() || layouts.isEmpty()) {
        QMessageBox::information(context.parent, tr("Export to HTML"),
                                 tr("There is no layout for this kind of project."));
        return;
    }
    QList<HtmlCommand> commands;
    if (kind == QLatin1String("char")) {
        const QString path = project->editorFiles(QStringLiteral("commands")).value(0);
        QString text;
        if (auto* code = qobject_cast<CodeFile*>(project->file(path))) {
            text = code->document()->toPlainText();
        } else if (QFile file(path); file.open(QIODevice::ReadOnly)) {
            QString encoding;
            QByteArray bom;
            text = CodeFile::decode(file.readAll(), &encoding, &bom);
        }
        commands = htmlCommands(text);
    }
    ExportHtmlDialog dialog(layouts, kind == QLatin1String("stage"), commands, context.parent);
    if (dialog.exec() != QDialog::Accepted)
        return;
    const QString suggestion =
        project->isMemoryOnly()
            ? QString()
            : QDir(project->directory())
                  .filePath(QFileInfo(project->filePath()).completeBaseName() + QStringLiteral(".html"));
    const QString caption = tr("Save As");
    const QString filter = tr("HyperText Markup Language (*.html)");
    const QString path = context.saveFileName
                             ? context.saveFileName(caption, suggestion, filter)
                             : QFileDialog::getSaveFileName(context.parent, caption, suggestion, filter);
    if (path.isEmpty())
        return;
    QString error;
    if (!Mugen::exportHtml(*project, dialog.options(), path, &error))
        QMessageBox::warning(context.parent, tr("Export to HTML"), error);
}

QStringList MugenEngine::toolActions() const
{
    return {QStringLiteral("st_throw"), QStringLiteral("st_offset"), QStringLiteral("st_palfx"),
            QStringLiteral("tools_export_html")};
}

void MugenEngine::runTool(const QString& actionId, const ToolContext& context)
{
    if (!context.project || context.project->engine() != this)
        return;
    if (actionId == QLatin1String("tools_export_html")) {
        exportHtml(context);
        return;
    }
    if (actionId == QLatin1String("st_throw")) {
        ThrowCreatorDialog dialog(context.project, context.projects,
                                  dataDir() + QStringLiteral("/scripts/common/Throw.lua"), context.parent);
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

QImage MugenEngine::portrait(Project& project) const
{
    // Sprite 9000,1 of the character's sprite file (9000,0 without it), if the file is there.
    const QStringList files = project.editorFiles(QStringLiteral("sprites"));
    if (files.isEmpty() || files.first().isEmpty())
        return {};
    auto* sprites = qobject_cast<SpriteFile*>(project.file(files.first()));
    if (!sprites && QFileInfo::exists(files.first()))
        sprites = project.spriteFile(files.first(), QStringLiteral("sprites"));
    if (!sprites)
        return {};
    const SpriteDocument& doc = sprites->document();
    int index = doc.findSprite(9000, 1);
    if (index < 0)
        index = doc.findSprite(9000, 0);
    if (index < 0)
        return {};
    const Formats::Image rgba = doc.image(index).toRgba(true);
    if (rgba.pixelBuffer().empty())
        return {};
    return toQImage(rgba);
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

QString MugenEngine::syntaxFileTypeForText(const Project& project, const QString& editorId, const QString& text) const
{
    const QString type = syntaxFileType(project, editorId);
    if (editorId != QLatin1String("definitions") || text.isEmpty())
        return type;
    // A motif's select, fight or storyboard file tells what it is by its sections.
    DefFile def;
    def.parse(text);
    const QString detected = MugenProject::detectType(def);
    return detected == QLatin1String("char") ? type : detected;
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
    const QString fileType = syntaxFileTypeForText(project, editorId, text);
    const SyntaxDatabase* db = syntax();
    if (fileType.isEmpty() || !db || db->isEmpty())
        return {};
    return validateMugenCode(*db, fileType, project.isVersionDeclared() ? project.versionId() : QString(), text);
}

} // namespace OFS::Mugen
