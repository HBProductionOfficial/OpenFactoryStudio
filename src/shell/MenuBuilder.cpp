#include "shell/MenuBuilder.h"

#include "core/actions/ActionRegistry.h"

#include <QAction>
#include <QMenu>

namespace OFS {

MenuBuilder::MenuBuilder(ActionRegistry& actions)
    : m_actions(actions)
{}

MenuBuilder::~MenuBuilder()
{
    for (Slot* slot : std::as_const(m_slots)) {
        QObject::disconnect(slot->showConnection);
        QObject::disconnect(slot->destroyConnection);
        delete slot;
    }
}

void MenuBuilder::setDynamic(const QString& key, Populate populate)
{
    m_populate.insert(key, std::move(populate));
}

QMenu* MenuBuilder::build(const MenuSpec& spec, QWidget* parent)
{
    auto* menu = new QMenu(spec.title, parent);
    menu->setObjectName(spec.id);
    addItems(menu, spec.items);
    return menu;
}

void MenuBuilder::addItems(QMenu* menu, const QList<MenuItemSpec>& items)
{
    for (const MenuItemSpec& item : items) {
        switch (item.kind) {
        case MenuItemSpec::Kind::Action:
            if (QAction* action = m_actions.action(item.id))
                menu->addAction(action);
            break;
        case MenuItemSpec::Kind::Separator:
            menu->addSeparator();
            break;
        case MenuItemSpec::Kind::Submenu: {
            QMenu* sub = menu->addMenu(item.title);
            sub->setObjectName(item.id);
            m_submenus.insert(item.id, sub);
            addItems(sub, item.children);
            break;
        }
        case MenuItemSpec::Kind::Dynamic: {
            auto* slot = new Slot;
            slot->menu = menu;
            slot->key = item.id;
            slot->marker = menu->addSeparator();
            slot->marker->setVisible(false);
            m_slots << slot;
            slot->showConnection = QObject::connect(menu, &QMenu::aboutToShow, menu, [this, slot] { refill(*slot); });
            slot->destroyConnection = QObject::connect(menu, &QObject::destroyed, [this, slot] {
                m_slots.removeOne(slot);
                delete slot;
            });
            break;
        }
        }
    }
}

void MenuBuilder::refill(Slot& slot)
{
    if (!slot.menu)
        return;
    for (QAction* a : std::as_const(slot.created)) {
        slot.menu->removeAction(a);
        if (a->parent() == slot.menu)
            delete a;
    }
    slot.created.clear();

    auto it = m_populate.constFind(slot.key);
    if (it == m_populate.cend())
        return;

    (*it)(slot.menu, slot.marker, slot.created);
}

} // namespace OFS
