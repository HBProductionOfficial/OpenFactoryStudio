#include "shell/dialogs/ShortcutEditor.h"

#include "core/actions/ActionRegistry.h"

#include <QAbstractTableModel>
#include <QAction>
#include <QColor>
#include <QHeaderView>
#include <QKeyEvent>
#include <QKeySequenceEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QSet>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QTimer>
#include <QVBoxLayout>

namespace OFS {

namespace {

constexpr int kFunctionColumn = 0;
constexpr int kKeysColumn = 1;

QString keyText(const QKeySequence& keys)
{
    return keys.toString(QKeySequence::PortableText);
}

} // namespace

class ShortcutModel : public QAbstractTableModel
{
public:
    explicit ShortcutModel(const ActionRegistry& actions, QObject* parent)
        : QAbstractTableModel(parent)
        , m_actions(actions)
    {}

    void refresh()
    {
        beginResetModel();
        m_ids = m_actions.ids();
        QHash<QString, QKeySequence> keys;
        for (const QString& id : std::as_const(m_ids))
            keys.insert(id, m_keys.value(id));
        m_keys = keys;
        updateConflicts();
        endResetModel();
    }

    const QStringList& ids() const { return m_ids; }
    QString id(int row) const { return row >= 0 && row < int(m_ids.size()) ? m_ids[row] : QString(); }
    int row(const QString& id) const { return int(m_ids.indexOf(id)); }
    QKeySequence keys(const QString& id) const { return m_keys.value(id); }
    const QHash<QString, QKeySequence>& table() const { return m_keys; }
    const QSet<QString>& conflicts() const { return m_conflicts; }

    void setTable(const QHash<QString, QKeySequence>& keys)
    {
        for (const QString& id : std::as_const(m_ids))
            m_keys.insert(id, keys.value(id));
        updateConflicts();
        if (!m_ids.isEmpty())
            Q_EMIT dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1));
    }

    void setKeys(const QString& id, const QKeySequence& keys)
    {
        if (!m_keys.contains(id))
            return;
        m_keys.insert(id, keys);
        updateConflicts();
        Q_EMIT dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1));
    }

    QStringList conflictsWith(const QString& id, const QKeySequence& keys) const
    {
        QStringList result;
        if (keys.isEmpty())
            return result;
        const QString text = keyText(keys);
        const QString scope = m_actions.scope(id);
        for (const QString& other : m_ids) {
            if (other != id && keyText(m_keys.value(other)) == text &&
                ActionRegistry::scopesOverlap(scope, m_actions.scope(other)))
                result << other;
        }
        return result;
    }

    int rowCount(const QModelIndex& parent = {}) const override { return parent.isValid() ? 0 : int(m_ids.size()); }
    int columnCount(const QModelIndex& parent = {}) const override { return parent.isValid() ? 0 : 2; }

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override
    {
        if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
            return QAbstractTableModel::headerData(section, orientation, role);
        return section == kFunctionColumn ? ShortcutEditor::tr("Function") : ShortcutEditor::tr("Keys");
    }

    QVariant data(const QModelIndex& index, int role) const override
    {
        if (!index.isValid() || index.row() >= rowCount())
            return {};
        const QString& id = m_ids[index.row()];
        switch (role) {
        case Qt::DisplayRole:
            if (index.column() == kFunctionColumn)
                return m_actions.description(id);
            return m_keys.value(id).toString(QKeySequence::NativeText);
        case Qt::EditRole:
            return index.column() == kKeysColumn ? QVariant::fromValue(m_keys.value(id)) : QVariant();
        case Qt::DecorationRole:
            if (index.column() == kFunctionColumn) {
                if (const QAction* action = m_actions.action(id))
                    return action->icon();
            }
            return {};
        case Qt::ToolTipRole:
            return id;
        case Qt::ForegroundRole:
            return m_conflicts.contains(id) ? QVariant(QColor(Qt::red)) : QVariant();
        default:
            return {};
        }
    }

    Qt::ItemFlags flags(const QModelIndex& index) const override
    {
        if (!index.isValid())
            return Qt::NoItemFlags;
        Qt::ItemFlags f = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
        if (index.column() == kKeysColumn)
            f |= Qt::ItemIsEditable;
        return f;
    }

private:
    void updateConflicts()
    {
        m_conflicts.clear();
        QHash<QString, QStringList> byKeys;
        for (const QString& id : std::as_const(m_ids)) {
            const QKeySequence keys = m_keys.value(id);
            if (!keys.isEmpty())
                byKeys[keyText(keys)] << id;
        }
        for (const QStringList& ids : std::as_const(byKeys)) {
            for (qsizetype i = 0; i < ids.size(); ++i) {
                for (qsizetype j = i + 1; j < ids.size(); ++j) {
                    if (ActionRegistry::scopesOverlap(m_actions.scope(ids[i]), m_actions.scope(ids[j]))) {
                        m_conflicts.insert(ids[i]);
                        m_conflicts.insert(ids[j]);
                    }
                }
            }
        }
    }

    const ActionRegistry& m_actions;
    QStringList m_ids;
    QHash<QString, QKeySequence> m_keys;
    QSet<QString> m_conflicts;
};

namespace {

// Edits the Keys column with a single-chord key sequence editor. The assignment, and the conflict question it may ask,
// runs after the editor has closed so a message box never interrupts it.
class KeysDelegate : public QStyledItemDelegate
{
public:
    KeysDelegate(ShortcutEditor* editor, ShortcutModel* model)
        : QStyledItemDelegate(editor)
        , m_editor(editor)
        , m_model(model)
    {}

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem&, const QModelIndex&) const override
    {
        auto* edit = new QKeySequenceEdit(parent);
        edit->setMaximumSequenceLength(1);
        edit->setClearButtonEnabled(true);
        auto* self = const_cast<KeysDelegate*>(this);
        QObject::connect(edit, &QKeySequenceEdit::editingFinished, self, [self, edit] {
            Q_EMIT self->commitData(edit);
            Q_EMIT self->closeEditor(edit);
        });
        return edit;
    }

    void setEditorData(QWidget* editor, const QModelIndex& index) const override
    {
        if (auto* edit = qobject_cast<QKeySequenceEdit*>(editor))
            edit->setKeySequence(m_model->keys(m_model->id(index.row())));
    }

    void setModelData(QWidget* editor, QAbstractItemModel*, const QModelIndex& index) const override
    {
        auto* edit = qobject_cast<QKeySequenceEdit*>(editor);
        if (!edit)
            return;
        const QString id = m_model->id(index.row());
        const QKeySequence keys = edit->keySequence();
        const QString request = id + QLatin1Char('\n') + keyText(keys);
        // The editor commits on editingFinished and again when it loses focus, so queue once.
        if (id.isEmpty() || keyText(m_model->keys(id)) == keyText(keys) || m_queued.contains(request))
            return;
        m_queued.insert(request);
        QPointer<ShortcutEditor> target = m_editor;
        QTimer::singleShot(0, m_editor, [this, target, id, keys, request] {
            m_queued.remove(request);
            if (target)
                target->assign(id, keys);
        });
    }

private:
    ShortcutEditor* m_editor;
    ShortcutModel* m_model;
    mutable QSet<QString> m_queued;
};

} // namespace

ShortcutEditor::ShortcutEditor(const ActionRegistry& actions, QWidget* parent)
    : QWidget(parent)
    , m_actions(actions)
    , m_model(new ShortcutModel(actions, this))
    , m_view(new QTableView(this))
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_view);

    m_view->setObjectName(QStringLiteral("shortcutTable"));
    m_view->setModel(m_model);
    m_view->setItemDelegateForColumn(kKeysColumn, new KeysDelegate(this, m_model));
    m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_view->setSelectionMode(QAbstractItemView::SingleSelection);
    m_view->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed |
                            QAbstractItemView::SelectedClicked);
    m_view->setTabKeyNavigation(false);
    m_view->setWordWrap(false);
    m_view->setContextMenuPolicy(Qt::CustomContextMenu);
    m_view->verticalHeader()->hide();
    m_view->horizontalHeader()->setStretchLastSection(false);
    m_view->horizontalHeader()->setHighlightSections(false);
    m_view->setColumnWidth(kFunctionColumn, 250);
    m_view->setColumnWidth(kKeysColumn, 150);
    m_view->installEventFilter(this);
    connect(m_view, &QWidget::customContextMenuRequested, this, &ShortcutEditor::showContextMenu);

    refreshActions();
}

ShortcutEditor::~ShortcutEditor() = default;

void ShortcutEditor::refreshActions()
{
    m_model->refresh();
}

QStringList ShortcutEditor::actionIds() const
{
    return m_model->ids();
}

void ShortcutEditor::setKeysTable(const QHash<QString, QKeySequence>& keys)
{
    m_model->setTable(keys);
}

QHash<QString, QKeySequence> ShortcutEditor::keysTable() const
{
    return m_model->table();
}

void ShortcutEditor::setDefaultKeys(const QHash<QString, QKeySequence>& keys)
{
    m_defaults = keys;
}

QKeySequence ShortcutEditor::keys(const QString& id) const
{
    return m_model->keys(id);
}

bool ShortcutEditor::assign(const QString& id, const QKeySequence& keys)
{
    if (m_model->row(id) < 0)
        return false;
    if (keyText(m_model->keys(id)) == keyText(keys))
        return true;
    const QStringList others = m_model->conflictsWith(id, keys);
    if (!others.isEmpty()) {
        if (!askConflict(id, keys, others))
            return false;
        for (const QString& other : others)
            m_model->setKeys(other, QKeySequence());
    }
    m_model->setKeys(id, keys);
    Q_EMIT changed();
    return true;
}

void ShortcutEditor::clear(const QString& id)
{
    if (m_model->row(id) < 0 || m_model->keys(id).isEmpty())
        return;
    m_model->setKeys(id, QKeySequence());
    Q_EMIT changed();
}

void ShortcutEditor::restoreDefault(const QString& id)
{
    assign(id, m_defaults.value(id));
}

QStringList ShortcutEditor::conflictsWith(const QString& id, const QKeySequence& keys) const
{
    return m_model->conflictsWith(id, keys);
}

QStringList ShortcutEditor::conflictingActions() const
{
    QStringList ids;
    for (const QString& id : m_model->ids()) {
        if (m_model->conflicts().contains(id))
            ids << id;
    }
    return ids;
}

void ShortcutEditor::setConflictHandler(ConflictHandler handler)
{
    m_conflictHandler = std::move(handler);
}

bool ShortcutEditor::askConflict(const QString& id, const QKeySequence& keys, const QStringList& others)
{
    if (m_conflictHandler)
        return m_conflictHandler(id, keys, others);
    QStringList names;
    for (const QString& other : others)
        names << QStringLiteral("- ") + m_actions.description(other);
    const QString text =
        tr("%1 is already used by:\n%2\n\nUse it for \"%3\" instead?")
            .arg(keys.toString(QKeySequence::NativeText), names.join(QLatin1Char('\n')), m_actions.description(id));
    return QMessageBox::question(this, tr("Keyboard shortcuts"), text) == QMessageBox::Yes;
}

QString ShortcutEditor::currentId() const
{
    return m_model->id(m_view->currentIndex().row());
}

bool ShortcutEditor::eventFilter(QObject* watched, QEvent* event)
{
    // Key presses reach the view only while no editor is open, because the editor takes them.
    if (watched == m_view && event->type() == QEvent::KeyPress) {
        const auto* key = static_cast<QKeyEvent*>(event);
        if ((key->key() == Qt::Key_Delete || key->key() == Qt::Key_Backspace) && key->modifiers() == Qt::NoModifier) {
            const QString id = currentId();
            if (!id.isEmpty()) {
                clear(id);
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void ShortcutEditor::showContextMenu(const QPoint& pos)
{
    const QModelIndex index = m_view->indexAt(pos);
    if (!index.isValid())
        return;
    const QString id = m_model->id(index.row());
    QMenu menu(this);
    QAction* edit = menu.addAction(tr("Edit"));
    QAction* clearAction = menu.addAction(tr("Clear"));
    clearAction->setEnabled(!m_model->keys(id).isEmpty());
    QAction* restore = menu.addAction(tr("Restore default"));
    restore->setEnabled(keyText(m_defaults.value(id)) != keyText(m_model->keys(id)));
    QAction* chosen = menu.exec(m_view->viewport()->mapToGlobal(pos));
    if (chosen == edit)
        m_view->edit(m_model->index(index.row(), kKeysColumn));
    else if (chosen == clearAction)
        clear(id);
    else if (chosen == restore)
        restoreDefault(id);
}

} // namespace OFS
