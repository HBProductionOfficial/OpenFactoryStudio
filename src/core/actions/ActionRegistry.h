#pragma once

#include "core/CoreExport.h"

#include <QHash>
#include <QKeySequence>
#include <QObject>
#include <QString>
#include <QStringList>

class QAction;

namespace OFS {

class IconSet;
class ShortcutSettings;

// Every command is a QAction registered here under a stable id ("file_open", "spr_add"...).
//
// Scopes let editors reuse a shortcut (Ctrl+A adds a sprite in Sprites and an animation in
// Animations):
//   ""               global, always active
//   "editor:<id>"    active only while that editor is current
//   "window:<id>"    a separate window like the image editor adds the action to itself, and the
//                    shortcut works only there
class OFS_CORE_EXPORT ActionRegistry : public QObject
{
    Q_OBJECT

public:
    explicit ActionRegistry(QObject* parent = nullptr);
    ~ActionRegistry() override;

    struct Spec
    {
        QString id;
        QString text;
        QString description;
        QString scope;
        bool checkable = false;
        QString iconName;
    };

    QAction* add(const Spec& spec);
    QAction* add(const QString& id, const QString& text, const QString& scope = {}, bool checkable = false);

    QAction* action(const QString& id) const;
    bool contains(const QString& id) const { return m_actions.contains(id); }
    QStringList ids() const;
    QString scope(const QString& id) const;
    QString caption(const QString& id) const;
    QString description(const QString& id) const;
    QList<QAction*> actionsInScope(const QString& scope) const;

    // Menus show the shortcut in parentheses in the shortcut column: "Open...", tab, " (Ctrl+O)".
    static QString displayText(const QString& caption, const QKeySequence& keys);
    static QString toolTipText(const QString& caption, const QKeySequence& keys);

    QKeySequence keys(const QString& id) const;
    void setKeys(const QString& id, const QKeySequence& keys);
    void applyShortcuts(const ShortcutSettings& shortcuts);
    void applyIcons(const IconSet& icons);

    void setActiveEditorScope(const QString& editorScope);
    QString activeEditorScope() const { return m_activeEditorScope; }

    QStringList conflicts() const;

private:
    bool isScopeActive(const QString& scope) const;
    void refreshShortcut(const QString& id);
    void refreshText(const QString& id);

    struct Entry
    {
        QAction* action = nullptr;
        QString caption;
        QString description;
        QString scope;
        QString iconName;
        QKeySequence keys;
    };
    QHash<QString, Entry> m_actions;
    QStringList m_order;
    QString m_activeEditorScope;
};

} // namespace OFS
