#include "core/engine/Engine.h"

#include "core/code/CodeOutline.h"
#include "core/code/Snippet.h"
#include "core/code/VariableUsage.h"

#include "core/app/Paths.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace OFS {

Engine::Engine(QObject* parent)
    : QObject(parent)
{}

Engine::~Engine() = default;

void Engine::initialize(AppContext&) {}

EditorInfo Engine::editor(const QString& id) const
{
    for (const EditorInfo& e : editors()) {
        if (e.id == id)
            return e;
    }
    return {};
}

Project* Engine::newProject(const QString& typeId, const QString& folder, QString* error)
{
    Q_UNUSED(folder)
    if (error)
        *error = tr("%1 has no blank project of type '%2'.").arg(displayName(), typeId);
    return nullptr;
}

QString Engine::scriptName() const
{
    return name().toUpper();
}

QMap<QString, QVariantMap> Engine::scriptEnums() const
{
    QVariantMap types;
    for (const ProjectTypeInfo& type : projectTypes())
        types.insert(type.id, type.id);
    return {{QStringLiteral("ProjectType"), types}};
}

QString Engine::projectFileFilter() const
{
    QStringList patterns;
    for (const QString& ext : projectExtensions())
        patterns << QStringLiteral("*.") + ext;
    return QStringLiteral("%1 (%2)").arg(displayName(), patterns.join(QLatin1Char(' ')));
}

bool Engine::canOpen(const QString& filePath) const
{
    return projectExtensions().contains(QFileInfo(filePath).suffix(), Qt::CaseInsensitive);
}

QString Engine::syntaxFileType(const Project&, const QString&) const
{
    return {};
}

IssueList Engine::validate(const Project&, const QString&, const QString&) const
{
    return {};
}

QStringList Engine::documentationVariants(const Project& project) const
{
    Q_UNUSED(project);
    return {QStringLiteral("common")};
}

QString Engine::documentationPage(const Project& project, const QString& kind, const QString& name) const
{
    if (kind.isEmpty() || name.isEmpty())
        return {};
    const QString file = kind.toLower() + QLatin1Char('.') + name.toLower() + QStringLiteral(".html");
    QStringList languages{m_language};
    if (m_language.compare(QLatin1String("en-us"), Qt::CaseInsensitive) != 0)
        languages << QStringLiteral("en-us");
    for (const QString& root : {userDataDir(), dataDir()}) {
        for (const QString& language : std::as_const(languages)) {
            for (const QString& variant : documentationVariants(project)) {
                const QString path = QStringLiteral("%1/docs/%2/%3/%4").arg(root, language, variant, file);
                if (QFileInfo::exists(path))
                    return path;
            }
        }
    }
    return {};
}

void RunCopy::remove() const
{
    for (auto it = files.crbegin(); it != files.crend(); ++it)
        QFile::remove(*it);
    for (auto it = folders.crbegin(); it != folders.crend(); ++it)
        QDir().rmdir(*it); // only when empty: files the copy did not create stay
}

RunCopy Engine::prepareRun(const Project& project, const QString& engineDir) const
{
    Q_UNUSED(project);
    Q_UNUSED(engineDir);
    RunCopy copy;
    copy.error = tr("This engine cannot run projects.");
    return copy;
}

QString Engine::runLogFile(const QString& engineDir) const
{
    Q_UNUSED(engineDir);
    return {};
}

QList<OutlineNode> Engine::outline(const Project& project, const QString& editorId, const QString& text) const
{
    Q_UNUSED(project);
    Q_UNUSED(editorId);
    return sectionOutline(text);
}

void Engine::scaleContactBoxes(Project& project, const QList<QPair<int, int>>& sprites, double widthScale,
                               double heightScale)
{
    Q_UNUSED(project);
    Q_UNUSED(sprites);
    Q_UNUSED(widthScale);
    Q_UNUSED(heightScale);
}

void Engine::runTool(const QString& actionId, const ToolContext& context)
{
    Q_UNUSED(actionId);
    Q_UNUSED(context);
}

VariableUsage Engine::variableUsage(const Project& project, const QString& editorId, const QString& text) const
{
    Q_UNUSED(project);
    Q_UNUSED(editorId);
    Q_UNUSED(text);
    return {};
}

QStringList Engine::codeTemplateDirs(const QString& editorId) const
{
    Q_UNUSED(editorId);
    return {};
}

SnippetLibrary& Engine::snippets()
{
    if (!m_snippets) {
        m_snippets = std::make_unique<SnippetLibrary>();
        reloadSnippets();
    }
    return *m_snippets;
}

void Engine::reloadSnippets()
{
    if (!m_snippets)
        m_snippets = std::make_unique<SnippetLibrary>();
    m_snippets->load({dataDir() + QStringLiteral("/snippets"), userSnippetDir()});
}

QString Engine::userSnippetDir() const
{
    return userDataDir() + QStringLiteral("/snippets");
}

QString Engine::dataDir() const
{
    return Paths::engineDataDir(name());
}

QString Engine::userDataDir() const
{
    return Paths::userEngineDir(name());
}

} // namespace OFS
