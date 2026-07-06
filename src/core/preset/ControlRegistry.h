#pragma once

#include "core/CoreExport.h"
#include "core/preset/PresetModel.h"

#include <QHash>
#include <QString>

#include <functional>

class QAction;
class QWidget;

namespace OFS {

class ActionRegistry;

// Creates the widgets that preset scripts place by name ("spr_sel", "anim_play", ...).
//
// Names without a factory that match an action become tool buttons for that action. Unknown
// names become disabled placeholders and are reported, so a preset never fails to load.
class OFS_CORE_EXPORT ControlRegistry
{
public:
    using Factory = std::function<QWidget*(const Preset::Control& control, QWidget* parent)>;

    explicit ControlRegistry(ActionRegistry* actions = nullptr);

    void registerFactory(const QString& name, Factory factory);
    void unregisterFactory(const QString& name);
    bool hasFactory(const QString& name) const { return m_factories.contains(name); }

    bool isKnown(const QString& name) const;

    QWidget* create(const Preset::Control& control, QWidget* parent, int defaultIconSize = 0) const;

    static QWidget* createActionButton(QAction* action, const Preset::Control& control, QWidget* parent,
                                       int defaultIconSize);

private:
    ActionRegistry* m_actions;
    QHash<QString, Factory> m_factories;
};

} // namespace OFS
