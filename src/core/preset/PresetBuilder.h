#pragma once

#include "core/CoreExport.h"
#include "core/preset/PresetModel.h"

#include <QStringList>

class QLayout;
class QToolBar;
class QWidget;

namespace OFS {

class ActionRegistry;
class ControlRegistry;

class OFS_CORE_EXPORT PresetBuilder
{
public:
    PresetBuilder(const ControlRegistry& controls, const ActionRegistry& actions);

    void buildPanel(const Preset::Layout& layout, QWidget* container, int defaultIconSize = 0);

    // Unknown actions are skipped and reported in warnings().
    void buildToolBar(const Preset::ToolBar& model, QToolBar* toolBar);

    const QStringList& warnings() const { return m_warnings; }

private:
    QLayout* buildLayout(const Preset::Layout& layout, QWidget* parent, int defaultIconSize);

    const ControlRegistry& m_controls;
    const ActionRegistry& m_actions;
    QStringList m_warnings;
};

} // namespace OFS
