#pragma once

#include "core/CoreExport.h"
#include "core/actions/MenuSpec.h"
#include "core/editors/EditorInfo.h"
#include "core/syntax/Issue.h"

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QtPlugin>

#include <memory>

namespace OFS {

class AppContext;
class CompletionModels;
class Project;
class SnippetLibrary;
class SyntaxDatabase;

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

    virtual const CompletionModels* completion() const { return nullptr; }

    virtual QString documentationPage(const Project& project, const QString& kind, const QString& name) const;

    void setLanguage(const QString& language) { m_language = language; }
    QString language() const { return m_language; }

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
