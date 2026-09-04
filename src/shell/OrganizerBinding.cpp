#include "shell/OrganizerBinding.h"

#include "core/editors/Organize.h"

#include <QDropEvent>
#include <QListView>
#include <QStringListModel>

#include <algorithm>

namespace OFS {

namespace {

// Turns the drops of a list into "dropped before row n".
class DropFilter : public QObject
{
public:
    DropFilter(QListView* list, QObject* owner, std::function<void(int)> dropped)
        : QObject(list)
        , m_list(list)
        , m_owner(owner)
        , m_dropped(std::move(dropped))
    {}

    QObject* owner() const { return m_owner; }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (watched != m_list->viewport() || event->type() != QEvent::Drop)
            return QObject::eventFilter(watched, event);
        auto* drop = static_cast<QDropEvent*>(event);
        // Drop before the item under the pointer, or after it on its lower half. Below the rows means the end.
        const QPoint position = drop->position().toPoint();
        const QModelIndex index = m_list->indexAt(position);
        int target = m_list->model() ? m_list->model()->rowCount() : 0;
        if (index.isValid())
            target = index.row() + (position.y() > m_list->visualRect(index).center().y() ? 1 : 0);
        drop->setDropAction(Qt::IgnoreAction); // the list itself does not move anything
        drop->accept();
        m_dropped(target);
        return true;
    }

private:
    QListView* m_list;
    QPointer<QObject> m_owner;
    std::function<void(int)> m_dropped;
};

DropFilter* currentFilter(QListView* list)
{
    return dynamic_cast<DropFilter*>(list->property("organizerDrops").value<QObject*>());
}

} // namespace

namespace OrganizerDrops {

void enable(QListView* list, QObject* owner, std::function<void(int target)> dropped)
{
    if (!list)
        return;
    delete currentFilter(list);
    auto* filter = new DropFilter(list, owner, std::move(dropped));
    list->setProperty("organizerDrops", QVariant::fromValue<QObject*>(filter));
    list->setDragDropMode(QAbstractItemView::InternalMove);
    list->setDefaultDropAction(Qt::MoveAction);
    list->setDropIndicatorShown(true);
    // Qt only sends drops to a widget that accepts them.
    list->viewport()->setAcceptDrops(true);
    list->viewport()->installEventFilter(filter);
}

void disable(QListView* list, QObject* owner)
{
    if (!list)
        return;
    DropFilter* filter = currentFilter(list);
    if (!filter || filter->owner() != owner)
        return;
    delete filter;
    list->setProperty("organizerDrops", QVariant());
    list->setDragDropMode(QAbstractItemView::NoDragDrop);
}

} // namespace OrganizerDrops

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
    if (!active) {
        OrganizerDrops::disable(m_list, this);
        return;
    }
    m_list->setModel(m_model);
    m_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_list->setEditTriggers(QAbstractItemView::NoEditTriggers);
    OrganizerDrops::enable(m_list, this, [this](int target) { dropSelection(target); });
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
