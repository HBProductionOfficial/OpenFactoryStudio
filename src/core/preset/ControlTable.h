#pragma once

#include "core/CoreExport.h"
#include "core/preset/PresetModel.h"

class QWidget;

namespace OFS {

class ControlRegistry;

enum class ControlKind
{
    Button,
    Label,
    Int,
    Float,
    Selector,
    Slider,
    Check,
    Combo,
    LineEdit,
    TextEdit,
    Group, // titled group box; presets fill it with :layout()
    Tree,
    List,
    Palette,
    Widget,
};

struct ControlDef
{
    const char* id;
    ControlKind kind;
    const char* caption; // translated in the "Controls" context
    int minimum;         // 0..0 = default range
    int maximum;
    bool checked;
    // Buttons and check boxes show the caption when these are empty. A check box's tooltip is
    // always its caption.
    const char* toolTip = nullptr;
    const char* statusTip = nullptr;
};

OFS_CORE_EXPORT QWidget* createControlWidget(const ControlDef& def, QWidget* parent);

OFS_CORE_EXPORT void registerControlTable(ControlRegistry& registry, const ControlDef* defs, int count);

template <int N>
void registerControlTable(ControlRegistry& registry, const ControlDef (&defs)[N])
{
    registerControlTable(registry, defs, N);
}

} // namespace OFS
