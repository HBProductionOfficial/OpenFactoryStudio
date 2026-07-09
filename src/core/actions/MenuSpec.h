#pragma once

#include "core/CoreExport.h"

#include <QList>
#include <QString>

namespace OFS {

struct MenuItemSpec
{
    enum class Kind
    {
        Action,
        Separator,
        Submenu, // id = object name, title, children
        Dynamic, // id = key of a list filled at runtime ("recent_projects", "encodings", ...)
    };

    Kind kind = Kind::Action;
    QString id;
    QString title;
    QList<MenuItemSpec> children;

    static MenuItemSpec action(const QString& id) { return {Kind::Action, id, {}, {}}; }
    static MenuItemSpec separator() { return {Kind::Separator, {}, {}, {}}; }
    static MenuItemSpec submenu(const QString& id, const QString& title, const QList<MenuItemSpec>& children)
    {
        return {Kind::Submenu, id, title, children};
    }
    static MenuItemSpec dynamic(const QString& key) { return {Kind::Dynamic, key, {}, {}}; }
};

struct MenuSpec
{
    QString id;
    QString title;
    QList<MenuItemSpec> items;
    bool hidden = false; // not in the menu bar, but its actions still work
};

// Engine menus go between the Debug and Tools menus.
OFS_CORE_EXPORT QList<MenuSpec> applicationMenusBeforeEngine();
OFS_CORE_EXPORT QList<MenuSpec> applicationMenusAfterEngine();

} // namespace OFS
