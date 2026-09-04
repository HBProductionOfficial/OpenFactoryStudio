#pragma once

#include <QList>
#include <QObject>
#include <QPointer>
#include <QStringList>

#include <functional>

class QListView;
class QStringListModel;
class QWidget;

namespace OFS {

// Drag and drop in the shared organizer list. `dropped` gets the row they were dropped before, or the row count for the
// end.
namespace OrganizerDrops {
void enable(QListView* list, QObject* owner, std::function<void(int target)> dropped);
void disable(QListView* list, QObject* owner);
} // namespace OrganizerDrops

// Organizer panel list for an editor whose items are reordered as a whole (animations, sounds, commands).
class OrganizerBinding : public QObject
{
    Q_OBJECT

public:
    struct Source
    {
        std::function<QStringList()> captions;
        // Applies a new order as one undo step. order[i] is the old position of the item now at i.
        std::function<void(const QList<int>& order)> reorder;
        std::function<QList<int>()> sortedOrder;
        std::function<void(int row)> activate;
    };

    explicit OrganizerBinding(Source source, QObject* parent = nullptr);

    void bind(QWidget* panel);
    void setActive(bool active);
    bool isActive() const { return m_active; }
    void refresh();
    bool perform(const QString& actionId);
    QList<int> selection() const;
    void dropSelection(int target);

private:
    void select(const QList<int>& rows);
    void apply(const QList<int>& order, const QList<int>& moved);

    Source m_source;
    QPointer<QListView> m_list;
    QStringListModel* m_model;
    bool m_active = false;
};

} // namespace OFS
