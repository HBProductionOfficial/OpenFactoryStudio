#pragma once

#include <QStringList>
#include <QWidget>

class QListWidget;
class QListWidgetItem;
class QToolBar;

namespace OFS {

class ActionRegistry;

// State controller list beside the States editor's code. It lists the engine's templates, where the user's own override
// shipped ones with the same name. Double-clicking a template inserts its text.
class ControllerTemplatesWidget : public QWidget
{
    Q_OBJECT

public:
    // `dirs` lists the template folders, shipped first. New and edited templates go to the last one.
    ControllerTemplatesWidget(ActionRegistry& actions, const QStringList& dirs, QWidget* parent = nullptr);

    void reload();
    QStringList names() const;
    QString selectedName() const;
    void selectName(const QString& name);

    QString templateText(const QString& name) const;
    bool saveTemplateAs(const QString& filePath, const QString& text, QString* error = nullptr);
    bool saveTemplate(const QString& name, const QString& text, QString* error = nullptr);
    QString userDir() const;
    bool deleteTemplate(const QString& name, QString* error = nullptr);
    QString templatePath(const QString& name) const;

    QListWidget* list() const { return m_list; }

Q_SIGNALS:
    void insertRequested(const QString& text);

private:
    QStringList m_dirs;
    QToolBar* m_toolBar;
    QListWidget* m_list;
};

} // namespace OFS
