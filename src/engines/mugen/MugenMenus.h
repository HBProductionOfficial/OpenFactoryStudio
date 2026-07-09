#pragma once

#include "core/actions/MenuSpec.h"

namespace OFS::Mugen {

// Menus of the M.U.G.E.N editors, shown between the Debug and Tools menus.
QList<MenuSpec> mugenMenus();
QList<MenuItemSpec> mugenToolsMenuItems();

} // namespace OFS::Mugen
