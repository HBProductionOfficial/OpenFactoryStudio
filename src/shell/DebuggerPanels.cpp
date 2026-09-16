#include "shell/DebuggerPanels.h"

#include "core/actions/ActionRegistry.h"
#include "core/debug/Breakpoints.h"
#include "core/theme/IconSet.h"
#include "shell/BottomDock.h"

#include <QAction>
#include <QDir>
#include <QMenu>
#include <QStandardItemModel>
#include <QTreeView>

namespace OFS {

using Debug::Breakpoint;
using Debug::BreakpointType;

DebuggerPanels::DebuggerPanels(BottomDock& dock, Debug::BreakpointList& breakpoints, ActionRegistry& actions,
                               const IconSet& icons, QObject* parent)
    : QObject(parent)
    , m_dock(dock)
    , m_breakpoints(breakpoints)
    , m_actions(actions)
    , m_icons(&icons)
{
    connect(&m_breakpoints, &Debug::BreakpointList::changed, this, &DebuggerPanels::refresh);
    QTreeView* list = m_dock.breakpointsView();
    connect(list, &QTreeView::customContextMenuRequested, this, &DebuggerPanels::showBreakpointMenu);
    connect(list, &QTreeView::doubleClicked, this, [this] {
        if (const int number = selectedBreakpoint())
            Q_EMIT editRequested(number);
    });
    connect(list->selectionModel(), &QItemSelectionModel::selectionChanged, this, &DebuggerPanels::updateActions);
    for (int i = 0; i < 2; ++i) {
        QTreeView* watch = m_dock.watchView(i);
        connect(watch, &QTreeView::customContextMenuRequested, this,
                [this, watch](const QPoint& pos) { showWatchMenu(watch, pos); });
    }
    refresh();
}

void DebuggerPanels::setSessionActive(bool active)
{
    if (m_sessionActive == active)
        return;
    m_sessionActive = active;
    refresh();
}

void DebuggerPanels::setIcons(const IconSet& icons)
{
    m_icons = &icons;
    refresh();
}

int DebuggerPanels::selectedBreakpoint() const
{
    const QModelIndex index = m_dock.breakpointsView()->currentIndex();
    if (!index.isValid() || !m_dock.breakpointsView()->selectionModel()->isSelected(index.siblingAtColumn(0)))
        return 0;
    return index.siblingAtColumn(0).data(Qt::UserRole).toInt();
}

void DebuggerPanels::refresh()
{
    // Missing texts show as "---" and missing numbers as 0, same as Fighter Factory.
    auto* model = qobject_cast<QStandardItemModel*>(m_dock.breakpointsView()->model());
    const int selected = selectedBreakpoint();
    model->removeRows(0, model->rowCount());
    auto text = [](const QString& value) {
        return value.isEmpty() ? QStringLiteral("---") : value;
    };
    for (const Breakpoint& b : m_breakpoints.breakpoints()) {
        QList<QStandardItem*> row;
        auto* number = new QStandardItem(QString::number(b.number));
        number->setData(b.number, Qt::UserRole);
        number->setIcon(m_icons->icon(!b.enabled          ? QStringLiteral("breakpoint_disabled")
                                      : !m_sessionActive  ? QStringLiteral("breakpoint_pending")
                                      : b.ignoreCount > 0 ? QStringLiteral("breakpoint_ignored")
                                                          : QStringLiteral("breakpoint_active")));
        row << number;
        row << new QStandardItem(text(b.function));
        row << new QStandardItem(
            text(b.type == BreakpointType::FileLine ? QDir::toNativeSeparators(b.file) : QString()));
        row << new QStandardItem(QString::number(b.type == BreakpointType::FileLine ? b.line : 0));
        row << new QStandardItem(b.address.isEmpty() ? QStringLiteral("0") : b.address);
        row << new QStandardItem(text(b.condition));
        row << new QStandardItem(QString::number(b.ignoreCount));
        model->appendRow(row);
        if (b.number == selected)
            m_dock.breakpointsView()->setCurrentIndex(number->index());
    }
    updateActions();
}

QStringList DebuggerPanels::breakpointRows() const
{
    QStringList rows;
    const QAbstractItemModel* model = m_dock.breakpointsView()->model();
    for (int r = 0; r < model->rowCount(); ++r) {
        QStringList cells;
        for (int c = 0; c < model->columnCount(); ++c)
            cells << model->index(r, c).data().toString();
        rows << cells.join(QLatin1Char('|'));
    }
    return rows;
}

void DebuggerPanels::updateActions()
{
    const int number = selectedBreakpoint();
    const Breakpoint* b = number ? m_breakpoints.find(number) : nullptr;
    if (QAction* a = m_actions.action(QStringLiteral("breakpoint_edit")))
        a->setEnabled(b != nullptr);
    if (QAction* a = m_actions.action(QStringLiteral("breakpoint_remove")))
        a->setEnabled(b != nullptr);
    if (QAction* a = m_actions.action(QStringLiteral("breakpoint_enable")))
        a->setEnabled(b && !b->enabled);
    if (QAction* a = m_actions.action(QStringLiteral("breakpoint_disable")))
        a->setEnabled(b && b->enabled);
    // Same as Fighter Factory, Remove all breakpoints stays enabled when there are none.
    if (QAction* a = m_actions.action(QStringLiteral("breakpoint_remove_all")))
        a->setEnabled(true);
}

void DebuggerPanels::showBreakpointMenu(const QPoint& pos)
{
    QTreeView* view = m_dock.breakpointsView();
    if (!view->indexAt(pos).isValid())
        view->clearSelection();
    updateActions();
    const int number = selectedBreakpoint();
    const Breakpoint* b = number ? m_breakpoints.find(number) : nullptr;
    QMenu menu(view);
    menu.addAction(m_actions.action(QStringLiteral("breakpoint_edit")));
    if (b)
        menu.addAction(
            m_actions.action(b->enabled ? QStringLiteral("breakpoint_disable") : QStringLiteral("breakpoint_enable")));
    menu.addAction(m_actions.action(QStringLiteral("breakpoint_remove")));
    menu.addAction(m_actions.action(QStringLiteral("breakpoint_remove_all")));
    menu.exec(view->viewport()->mapToGlobal(pos));
}

void DebuggerPanels::showWatchMenu(QTreeView* view, const QPoint& pos)
{
    if (!view->indexAt(pos).isValid())
        view->clearSelection();
    m_watchList = view == m_dock.watchView(1) ? 1 : 0;
    QAction* remove = m_actions.action(QStringLiteral("watch_remove"));
    QAction* removeAll = m_actions.action(QStringLiteral("watch_remove_all"));
    remove->setEnabled(view->selectionModel()->hasSelection());
    removeAll->setEnabled(view->model()->rowCount() > 0);
    QMenu menu(view);
    menu.addAction(remove);
    menu.addAction(removeAll);
    menu.exec(view->viewport()->mapToGlobal(pos));
}

} // namespace OFS
