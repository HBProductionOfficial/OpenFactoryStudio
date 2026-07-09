#pragma once

#include "core/CoreExport.h"

#include <QList>
#include <QString>

namespace OFS {

class ActionRegistry;

struct ActionDef
{
    const char* id;
    const char* text;
    const char* description;
    const char* scope; // "" global, "editor:<id>", "window:<id>"
    bool checkable;
};

// Texts are translated in the "Actions" context.
OFS_CORE_EXPORT void registerActions(ActionRegistry& registry, const ActionDef* defs, int count);

template <int N>
void registerActions(ActionRegistry& registry, const ActionDef (&defs)[N])
{
    registerActions(registry, defs, N);
}

OFS_CORE_EXPORT void registerApplicationActions(ActionRegistry& registry);

} // namespace OFS
