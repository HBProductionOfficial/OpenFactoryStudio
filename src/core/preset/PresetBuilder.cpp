#include "core/preset/PresetBuilder.h"

#include "core/actions/ActionRegistry.h"
#include "core/preset/ControlRegistry.h"

#include <QAction>
#include <QBoxLayout>
#include <QFrame>
#include <QGridLayout>
#include <QSpacerItem>
#include <QToolBar>
#include <QWidget>

namespace OFS {

PresetBuilder::PresetBuilder(const ControlRegistry& controls, const ActionRegistry& actions)
    : m_controls(controls)
    , m_actions(actions)
{}

void PresetBuilder::buildPanel(const Preset::Layout& layout, QWidget* container, int defaultIconSize)
{
    // The old controls leave the panel now but are deleted later, because a signal of one of them
    // may be running. Editors look up the new ones by name right after this.
    auto discard = [](QWidget* w) {
        w->hide();
        w->setParent(nullptr);
        w->deleteLater();
    };
    if (QLayout* old = container->layout()) {
        while (QLayoutItem* item = old->takeAt(0)) {
            if (QWidget* w = item->widget())
                discard(w);
            delete item;
        }
        delete old;
    }
    const QList<QWidget*> children = container->findChildren<QWidget*>(Qt::FindDirectChildrenOnly);
    for (QWidget* child : children)
        discard(child);

    QLayout* built = buildLayout(layout, container, defaultIconSize);
    container->setLayout(built);
}

QLayout* PresetBuilder::buildLayout(const Preset::Layout& layout, QWidget* parent, int defaultIconSize)
{
    using Kind = Preset::LayoutItem::Kind;

    QLayout* result = nullptr;
    QGridLayout* grid = nullptr;
    QBoxLayout* box = nullptr;
    switch (layout.type) {
    case Preset::Layout::Type::Grid:
        result = grid = new QGridLayout;
        break;
    case Preset::Layout::Type::Horizontal:
        result = box = new QHBoxLayout;
        break;
    case Preset::Layout::Type::Vertical:
        result = box = new QVBoxLayout;
        break;
    }
    result->setContentsMargins(layout.margin, layout.margin, layout.margin, layout.margin);
    result->setSpacing(layout.spacing);

    for (const Preset::LayoutItem& item : layout.items) {
        QWidget* widget = nullptr;
        QLayout* child = nullptr;
        QSpacerItem* spacer = nullptr;

        switch (item.kind) {
        case Kind::Control:
            if (!item.control)
                continue;
            if (!m_controls.isKnown(item.control->name))
                m_warnings << QStringLiteral("unknown control '%1'").arg(item.control->name);
            widget = m_controls.create(*item.control, parent, defaultIconSize);
            if (item.control->content) {
                QLayout* inner = buildLayout(*item.control->content, widget, defaultIconSize);
                if (QLayout* old = widget->layout())
                    delete old;
                widget->setLayout(inner);
            }
            break;
        case Kind::Layout:
            if (!item.layout)
                continue;
            child = buildLayout(*item.layout, parent, defaultIconSize);
            break;
        case Kind::Spacing:
            if (box)
                box->addSpacing(item.spacing);
            continue;
        case Kind::HorizontalSpacer:
            spacer = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);
            break;
        case Kind::VerticalSpacer:
            spacer = new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
            break;
        case Kind::Separator: {
            auto* line = new QFrame(parent);
            const bool vertical = box && box->direction() <= QBoxLayout::RightToLeft;
            line->setFrameShape(vertical ? QFrame::VLine : QFrame::HLine);
            line->setFrameShadow(QFrame::Sunken);
            widget = line;
            break;
        }
        }

        if (grid) {
            if (widget)
                grid->addWidget(widget, item.row, item.column, item.rowSpan, item.columnSpan);
            else if (child)
                grid->addLayout(child, item.row, item.column, item.rowSpan, item.columnSpan);
            else if (spacer)
                grid->addItem(spacer, item.row, item.column, item.rowSpan, item.columnSpan);
        } else if (box) {
            if (widget)
                box->addWidget(widget);
            else if (child)
                box->addLayout(child, item.stretch);
            else if (spacer)
                box->addItem(spacer);
        }
    }
    return result;
}

void PresetBuilder::buildToolBar(const Preset::ToolBar& model, QToolBar* toolBar)
{
    toolBar->clear();
    if (model.iconSize > 0)
        toolBar->setIconSize(QSize(model.iconSize, model.iconSize));
    for (const QString& id : model.actions) {
        if (id.isEmpty()) {
            toolBar->addSeparator();
            continue;
        }
        if (QAction* action = m_actions.action(id))
            toolBar->addAction(action);
        else
            m_warnings << QStringLiteral("toolbar '%1': unknown action '%2'").arg(model.name, id);
    }
}

} // namespace OFS
