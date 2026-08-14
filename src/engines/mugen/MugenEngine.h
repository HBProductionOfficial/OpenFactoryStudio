#pragma once

#include "core/engine/Engine.h"

#include <memory>

namespace OFS {
class SyntaxDatabase;
}

namespace OFS::Mugen {

class MugenEngine : public Engine
{
    Q_OBJECT

public:
    explicit MugenEngine(QObject* parent = nullptr);
    ~MugenEngine() override;

    QString name() const override;
    QString displayName() const override;
    QList<EngineVersion> versions() const override;
    QList<ProjectTypeInfo> projectTypes() const override;
    QStringList projectExtensions() const override;

    void initialize(AppContext& context) override;
    QList<EditorInfo> editors() const override;
    QList<MenuSpec> menus() const override;
    QList<MenuItemSpec> toolsMenuItems() const override;
    Project* openProject(const QString& filePath, QString* error) override;
    const SyntaxDatabase* syntax() const override;
    const CompletionModels* completion() const override;
    QStringList codeTemplateDirs(const QString& editorId) const override;
    RunCopy prepareRun(const Project& project, const QString& engineDir) const override;
    QString runLogFile(const QString& engineDir) const override;
    QList<OutlineNode> outline(const Project& project, const QString& editorId, const QString& text) const override;
    VariableUsage variableUsage(const Project& project, const QString& editorId, const QString& text) const override;
    QStringList toolActions() const override;
    void runTool(const QString& actionId, const ToolContext& context) override;

protected:
    QStringList documentationVariants(const Project& project) const override;

public:
    QString syntaxFileType(const Project& project, const QString& editorId) const override;
    IssueList validate(const Project& project, const QString& editorId, const QString& text) const override;

private:
    mutable std::unique_ptr<SyntaxDatabase> m_syntax;
    mutable std::unique_ptr<CompletionModels> m_completion;
};

} // namespace OFS::Mugen
