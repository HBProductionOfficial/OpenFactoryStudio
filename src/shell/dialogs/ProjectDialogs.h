#pragma once

#include "core/project/ProjectBackup.h"

#include <QDialog>
#include <QList>
#include <QString>

class QCheckBox;
class QComboBox;
class QListWidget;
class QRadioButton;
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

class BackupDialog : public QDialog
{
    Q_OBJECT

public:
    BackupDialog(const QList<Backup::Item>& backups, QWidget* parent = nullptr);

    QString selectedBackup() const;

private:
    QListWidget* m_list;
    QList<Backup::Item> m_backups;
};

class ItemImportDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Kind
    {
        Animations,
        Sounds,
    };
    enum class Conflict
    {
        Ask,
        Replace,
        Skip,
        AsIs,
    };
    struct Item
    {
        QString caption;    // in the source list: "7777 - Standing", "0, 77"
        QString identifier; // in the destination list: "7777", "0,77"
        int number = 0;     // for sorting: the action number, or group * 65536 + number
    };
    struct Source
    {
        QString name;
        QList<Item> items;
    };
    struct Chosen
    {
        int source = 0;
        QString identifier;
    };

    ItemImportDialog(Kind kind, const QList<Source>& projects, QWidget* parent = nullptr);

    int project() const;
    QList<Chosen> items() const;
    Conflict conflict() const;
    bool importSprites() const;

private:
    void fillSource();

    QList<Source> m_projects;
    QComboBox* m_project;
    QCheckBox* m_sorted;
    QListWidget* m_source;
    QListWidget* m_destination;
    QCheckBox* m_sprites = nullptr;
    QRadioButton* m_ask;
    QRadioButton* m_replace;
    QRadioButton* m_skip;
    QRadioButton* m_asIs;
};

} // namespace OFS
