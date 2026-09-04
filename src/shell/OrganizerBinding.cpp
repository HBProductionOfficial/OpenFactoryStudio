#include "shell/OrganizerBinding.h"

#include "core/editors/Organize.h"

#include <QDropEvent>
#include <QListView>
#include <QStringListModel>

#include <algorithm>

namespace OFS {

OrganizerBinding::OrganizerBinding(Source source, QObject* parent)
    : QObject(parent)
    , m_source(std::move(source))
    , m_model(new QStringListModel(this))
{}

void OrganizerBinding::bind(QWidget* panel)
{
    if (m_list)
        disconnect(m_list, nullptr, this, nullptr);
    m_list = panel ? panel->findChild<QListView*>(QStringLiteral("org_list")) : nullptr;
    setActive(m_active);
}

void OrganizerBinding::setActive(bool active)
{
    m_active = active;
    if (!m_list)
        return;
    disconnect(m_list, nullptr, this, nullptr);
    m_list->viewport()->removeEventFilter(this);
    if (!active) {
        m_list->setDragDropMode(QAbstractItemView::NoDragDrop);
        return;
    }
    m_list->setModel(m_model);
    m_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_list->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_list->setDragDropMode(QAbstractItemView::InternalMove);
    m_list->setDefaultDropAction(Qt::MoveAction);
    m_list->setDropIndicatorShown(true);
    // Qt only sends drops to a widget that accepts them.
    m_list->viewport()->setAcceptDrops(true);
    m_list->viewport()->installEventFilter(this);
    connect(m_list, &QListView::doubleClicked, this, [this](const QModelIndex& index) {
        if (m_source.activate)
            m_source.activate(index.row());
    });
    refresh();
}

void OrganizerBinding::refresh()
{
    if (!m_active || !m_list)
        return;
    const QList<int> selected = selection();
    const QStringList captions = m_source.captions ? m_source.captions() : QStringList();
    if (captions != m_model->stringList())
        m_model->setStringList(captions);
    select(selected);
}

QList<int> OrganizerBinding::selection() const
{
    QList<int> rows;
    if (m_list && m_list->selectionModel() && m_list->model() == m_model) {
        for (const QModelIndex& index : m_list->selectionModel()->selectedRows())
            rows << index.row();
    }
    std::sort(rows.begin(), rows.end());
    return rows;
}

void OrganizerBinding::select(const QList<int>& rows)
{
    if (!m_list || !m_list->selectionModel() || m_list->model() != m_model)
        return;
    m_list->selectionModel()->clearSelection();
    for (int row : rows) {
        if (row >= 0 && row < m_model->rowCount())
            m_list->selectionModel()->select(m_model->index(row), QItemSelectionModel::Select);
    }
}

bool OrganizerBinding::eventFilter(QObject* watched, QEvent* event)
{
    if (!m_list || watched != m_list->viewport() || event->type() != QEvent::Drop || !m_active ||
        m_list->model() != m_model)
        return QObject::eventFilter(watched, event);
    auto* drop = static_cast<QDropEvent*>(event);
    // Drop before the item under the pointer, or after it on its lower half. Below the rows means the end.
    const QPoint position = drop->position().toPoint();
    const QModelIndex index = m_list->indexAt(position);
    int target = m_model->rowCount();
    if (index.isValid()) {
        const QRect rect = m_list->visualRect(index);
        target = index.row() + (position.y() > rect.center().y() ? 1 : 0);
    }
    drop->setDropAction(Qt::IgnoreAction); // the list itself does not move anything
    drop->accept();
    dropSelection(target);
    return true;
}

void OrganizerBinding::dropSelection(int target)
{
    const QList<int> rows = selection();
    if (rows.isEmpty() || !m_source.reorder)
        return;
    QList<int> moved;
    const QList<int> order = Organize::dropOrder(m_model->rowCount(), rows, target, &moved);
    if (moved != rows)
        apply(order, moved);
}

void OrganizerBinding::apply(const QList<int>& order, const QList<int>& moved)
{
    m_source.reorder(order);
    refresh();
    select(moved);
}

bool OrganizerBinding::perform(const QString& actionId)
{
    if (!actionId.startsWith(QLatin1String("org_")))
        return false;
    const int count = m_model->rowCount();
    if (!m_source.reorder || count == 0)
        return true;
    if (actionId == QLatin1String("org_auto")) {
        const QList<int> order = m_source.sortedOrder ? m_source.sortedOrder() : QList<int>();
        bool changed = false;
        for (int i = 0; i < order.size(); ++i)
            changed = changed || order[i] != i;
        if (changed)
            m_source.reorder(order);
        return true;
    }
    const int delta = Organize::deltaOf(actionId, count);
    const QList<int> rows = selection();
    if (delta == 0 || rows.isEmpty())
        return true;
    QList<int> moved;
    const QList<int> order = Organize::moveOrder(count, rows, delta, &moved);
    if (moved == rows)
        return true;
    apply(order, moved);
    return true;
}

} // namespace OFS
