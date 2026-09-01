#pragma once

#include <QDialog>
#include <QList>
#include <QString>

class QTabWidget;

namespace OFS {

class EngineManager;

// A project template script: scripts/project/<category>/<name>.lua in an engine's data folder or
// in the user's, with an optional <name>.png (or .svg) icon beside it.
struct ProjectTemplate
{
    QString category;
    QString name;
    QString script;
    QString icon;
    QString engine;
};

QList<ProjectTemplate> findProjectTemplates(const EngineManager& engines);

class ProjectTemplateDialog : public QDialog
{
    Q_OBJECT

public:
    ProjectTemplateDialog(const EngineManager& engines, QWidget* parent = nullptr);

    ProjectTemplate selectedTemplate() const;

private:
    QTabWidget* m_tabs;
    QList<ProjectTemplate> m_templates;
};

} // namespace OFS
