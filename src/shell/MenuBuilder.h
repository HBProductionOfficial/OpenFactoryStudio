#pragma once

#include "core/actions/MenuSpec.h"

#include <QHash>
#include <QList>
#include <QMetaObject>
#include <QPointer>

#include <functional>

class QAction;
class QMenu;
class QWidget;

namespace OFS {

class ActionRegistry;

// Builds QMenus from MenuSpecs. Dynamic items are filled each time their menu is about to show.
class MenuBuilder
{
public:
    // Created actions are removed and deleted before the next refill. Actions owned elsewhere are only removed.
    using Populate = std::function<void(QMenu* menu, QAction* before, QList<QAction*>& created)>;

    explicit MenuBuilder(ActionRegistry& actions);
    ~MenuBuilder();
    MenuBuilder(const MenuBuilder&) = delete;
    MenuBuilder& operator=(const MenuBuilder&) = delete;

    void setDynamic(const QString& key, Populate populate);

    QMenu* build(const MenuSpec& spec, QWidget* parent);
    void addItems(QMenu* menu, const QList<MenuItemSpec>& items);

    QMenu* submenu(const QString& id) const { return m_submenus.value(id); }

private:
    struct Slot
    {
        QPointer<QMenu> menu;
        QString key;
        QAction* marker = nullptr;
        QList<QAction*> created;
        QMetaObject::Connection showConnection;
        QMetaObject::Connection destroyConnection;
    };

    void refill(Slot& slot);

    ActionRegistry& m_actions;
    QHash<QString, Populate> m_populate;
    QList<Slot*> m_slots;
    QHash<QString, QMenu*> m_submenus;
};

} // namespace OFS
