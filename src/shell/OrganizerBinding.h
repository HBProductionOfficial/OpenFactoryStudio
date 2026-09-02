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

private:
    void select(const QList<int>& rows);

    Source m_source;
    QPointer<QListView> m_list;
    QStringListModel* m_model;
    bool m_active = false;
};

} // namespace OFS
