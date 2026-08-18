#pragma once

#include "core/CoreExport.h"
#include "core/actions/MenuSpec.h"
#include "core/editors/EditorInfo.h"
#include "core/syntax/Issue.h"

#include <QIcon>
#include <QList>
#include <QObject>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QtPlugin>

#include <functional>
#include <memory>

class QWidget;

namespace OFS {

class AppContext;
class CompletionModels;
struct OutlineNode;
class Project;
class SnippetLibrary;
class SyntaxDatabase;
struct VariableUsage;

struct EngineVersion
{
    QString id;
    QString name;
};

struct ProjectTypeInfo
{
    QString id; // "char", "stage", "font", "storyboard", "motif", "fight" or "select"
    QString name;
};

// Copy of a project made in the engine folder for Run in engine.
struct OFS_CORE_EXPORT RunCopy
{
    QString name;
    QStringList files;
    QStringList folders;
    QString error;

    bool isValid() const { return error.isEmpty() && !name.isEmpty(); }
    void remove() const;
};

// What an engine's own tool dialogs (like the Throw creator) work on.
struct ToolContext
{
    QWidget* parent = nullptr;
    Project* project = nullptr;
    QList<Project*> projects;
    std::function<void(const QString&)> insertText;
    std::function<QIcon(const QString&)> icon;
};

// A game engine supported by the editor. Each one is a plugin under engines/<name>/ with its own
// data folder. The Engine plugins document in docs explains how plugins work.
class OFS_CORE_EXPORT Engine : public QObject
{
    Q_OBJECT

public:
    explicit Engine(QObject* parent = nullptr);
    ~Engine() override;

    // Short id, also the name of the data folder (data/engines/<name>).
    virtual QString name() const = 0;
    virtual QString displayName() const = 0;
    virtual QList<EngineVersion> versions() const = 0;
    virtual QList<ProjectTypeInfo> projectTypes() const = 0;

    virtual QStringList projectExtensions() const = 0;

    virtual void initialize(AppContext& context);

    virtual QList<EditorInfo> editors() const { return {}; }
    EditorInfo editor(const QString& id) const;

    virtual QList<MenuSpec> menus() const { return {}; }
    virtual QList<MenuItemSpec> toolsMenuItems() const { return {}; }

    virtual QString projectFileFilter() const;

    virtual bool canOpen(const QString& filePath) const;

    // The caller owns the project.
    virtual Project* openProject(const QString& filePath, QString* error) = 0;

    virtual const SyntaxDatabase* syntax() const { return nullptr; }
    virtual QString syntaxFileType(const Project& project, const QString& editorId) const;

    virtual IssueList validate(const Project& project, const QString& editorId, const QString& text) const;

    virtual QList<OutlineNode> outline(const Project& project, const QString& editorId, const QString& text) const;
    virtual VariableUsage variableUsage(const Project& project, const QString& editorId, const QString& text) const;

    virtual RunCopy prepareRun(const Project& project, const QString& engineDir) const;
    virtual QString runLogFile(const QString& engineDir) const;

    // Called after the Sprites editor resized sprites, so boxes drawn on them scale too.
    virtual void scaleContactBoxes(Project& project, const QList<QPair<int, int>>& sprites, double widthScale,
                                   double heightScale);
    virtual bool hasContactBoxes() const { return false; }

    virtual QStringList toolActions() const { return {}; }
    virtual void runTool(const QString& actionId, const ToolContext& context);

    virtual const CompletionModels* completion() const { return nullptr; }

    virtual QString documentationPage(const Project& project, const QString& kind, const QString& name) const;

    void setLanguage(const QString& language) { m_language = language; }
    QString language() const { return m_language; }

    virtual QStringList codeTemplateDirs(const QString& editorId) const;

    SnippetLibrary& snippets();
    void reloadSnippets();
    QString userSnippetDir() const;

    QString dataDir() const;
    QString userDataDir() const;

protected:
    virtual QStringList documentationVariants(const Project& project) const;

private:
    std::unique_ptr<SnippetLibrary> m_snippets;
    QString m_language = QStringLiteral("en-us");
};

class EnginePlugin
{
public:
    virtual ~EnginePlugin() = default;
    virtual Engine* createEngine(QObject* parent) = 0;
};

} // namespace OFS

#define OFS_ENGINE_PLUGIN_IID "org.openfactorystudio.EnginePlugin/1"
Q_DECLARE_INTERFACE(OFS::EnginePlugin, OFS_ENGINE_PLUGIN_IID)
