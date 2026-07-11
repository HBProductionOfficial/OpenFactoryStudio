#pragma once

#include <QHash>
#include <QKeySequence>
#include <QStringList>
#include <QWidget>

#include <functional>

class QTableView;

namespace OFS {

class ActionRegistry;
class ShortcutModel;

// Options > Interface > Keyboard shortcuts table, a working copy read back on Apply. Keys used by another action in an
// overlapping scope go through the conflict handler, and rows that still collide show in red.
class ShortcutEditor : public QWidget
{
    Q_OBJECT

public:
    // Return true to assign `keys` to `id` anyway. `others` are the ids that lose the sequence.
    using ConflictHandler = std::function<bool(const QString& id, const QKeySequence& keys, const QStringList& others)>;

    explicit ShortcutEditor(const ActionRegistry& actions, QWidget* parent = nullptr);
    ~ShortcutEditor() override;

    void refreshActions();
    QStringList actionIds() const;

    void setKeysTable(const QHash<QString, QKeySequence>& keys);
    QHash<QString, QKeySequence> keysTable() const;
    void setDefaultKeys(const QHash<QString, QKeySequence>& keys);

    QKeySequence keys(const QString& id) const;
    bool assign(const QString& id, const QKeySequence& keys);
    void clear(const QString& id);
    void restoreDefault(const QString& id);

    QStringList conflictsWith(const QString& id, const QKeySequence& keys) const;
    QStringList conflictingActions() const;

    void setConflictHandler(ConflictHandler handler);
    QTableView* view() const { return m_view; }

Q_SIGNALS:
    void changed();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QString currentId() const;
    void showContextMenu(const QPoint& pos);
    bool askConflict(const QString& id, const QKeySequence& keys, const QStringList& others);

    const ActionRegistry& m_actions;
    ShortcutModel* m_model;
    QTableView* m_view;
    QHash<QString, QKeySequence> m_defaults;
    ConflictHandler m_conflictHandler;
};

} // namespace OFS
