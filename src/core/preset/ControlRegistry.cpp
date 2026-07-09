#include "core/preset/ControlRegistry.h"

#include "core/actions/ActionRegistry.h"

#include <QAbstractSlider>
#include <QAction>
#include <QLabel>
#include <QToolButton>

namespace OFS {

ControlRegistry::ControlRegistry(ActionRegistry* actions)
    : m_actions(actions)
{}

void ControlRegistry::registerFactory(const QString& name, Factory factory)
{
    m_factories.insert(name, std::move(factory));
}

void ControlRegistry::unregisterFactory(const QString& name)
{
    m_factories.remove(name);
}

bool ControlRegistry::isKnown(const QString& name) const
{
    return m_factories.contains(name) || (m_actions && m_actions->contains(name));
}

QWidget* ControlRegistry::createActionButton(QAction* action, const Preset::Control& control, QWidget* parent,
                                             int defaultIconSize)
{
    auto* button = new QToolButton(parent);
    button->setObjectName(control.name);
    button->setDefaultAction(action);
    button->setAutoRaise(control.flat);
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    const int iconSize = control.iconSize > 0 ? control.iconSize : defaultIconSize;
    if (iconSize > 0)
        button->setIconSize(QSize(iconSize, iconSize));
    return button;
}

QWidget* ControlRegistry::create(const Preset::Control& control, QWidget* parent, int defaultIconSize) const
{
    QWidget* widget = nullptr;
    if (auto it = m_factories.constFind(control.name); it != m_factories.cend()) {
        widget = (*it)(control, parent);
    } else if (QAction* action = m_actions ? m_actions->action(control.name) : nullptr) {
        widget = createActionButton(action, control, parent, defaultIconSize);
    }

    if (!widget) {
        auto* placeholder = new QToolButton(parent);
        placeholder->setText(control.name);
        placeholder->setToolTip(control.name);
        placeholder->setEnabled(false);
        placeholder->setAutoRaise(true);
        widget = placeholder;
    }

    if (widget->objectName().isEmpty())
        widget->setObjectName(control.name);
    if (control.size.isValid()) {
        if (control.size.width() > 0)
            widget->setFixedWidth(control.size.width());
        if (control.size.height() > 0)
            widget->setFixedHeight(control.size.height());
    }
    if (control.orientation) {
        if (auto* slider = qobject_cast<QAbstractSlider*>(widget))
            slider->setOrientation(*control.orientation);
    }
    if (control.alignment) {
        if (auto* label = qobject_cast<QLabel*>(widget))
            label->setAlignment(control.alignment);
    }
    if (control.wordWrap) {
        if (auto* label = qobject_cast<QLabel*>(widget))
            label->setWordWrap(true);
    }
    if (control.iconSize > 0) {
        if (auto* button = qobject_cast<QToolButton*>(widget))
            button->setIconSize(QSize(control.iconSize, control.iconSize));
    }
    if (control.flat) {
        if (auto* button = qobject_cast<QToolButton*>(widget))
            button->setAutoRaise(true);
    }
    return widget;
}

} // namespace OFS
