#include "core/actions/ActionRegistry.h"

#include "core/settings/ShortcutSettings.h"
#include "core/theme/IconSet.h"

#include <QAction>
#include <QMap>

namespace OFS {

namespace {

bool isEditorScope(const QString& scope)
{
    return scope.startsWith(QLatin1String("editor:"));
}

bool isWindowScope(const QString& scope)
{
    return scope.startsWith(QLatin1String("window:"));
}

} // namespace

ActionRegistry::ActionRegistry(QObject* parent)
    : QObject(parent)
{}

ActionRegistry::~ActionRegistry() = default;

QAction* ActionRegistry::add(const Spec& spec)
{
    if (auto it = m_actions.find(spec.id); it != m_actions.end())
        return it->action;

    auto* action = new QAction(spec.text, this);
    action->setObjectName(spec.id);
    action->setCheckable(spec.checkable);
    action->setShortcutContext(isWindowScope(spec.scope) ? Qt::WindowShortcut : Qt::ApplicationShortcut);

    Entry entry;
    entry.action = action;
    entry.scope = spec.scope;
    entry.iconName = spec.iconName.isEmpty() ? spec.id : spec.iconName;
    m_actions.insert(spec.id, entry);
    m_order << spec.id;
    return action;
}

QAction* ActionRegistry::add(const QString& id, const QString& text, const QString& scope, bool checkable)
{
    return add(Spec{id, text, scope, checkable, {}});
}

QAction* ActionRegistry::action(const QString& id) const
{
    auto it = m_actions.constFind(id);
    return it != m_actions.cend() ? it->action : nullptr;
}

QStringList ActionRegistry::ids() const
{
    return m_order;
}

QString ActionRegistry::scope(const QString& id) const
{
    return m_actions.value(id).scope;
}

QList<QAction*> ActionRegistry::actionsInScope(const QString& scope) const
{
    QList<QAction*> list;
    for (const QString& id : m_order) {
        const Entry& e = m_actions[id];
        if (e.scope == scope)
            list << e.action;
    }
    return list;
}

QKeySequence ActionRegistry::keys(const QString& id) const
{
    return m_actions.value(id).keys;
}

void ActionRegistry::setKeys(const QString& id, const QKeySequence& keys)
{
    auto it = m_actions.find(id);
    if (it == m_actions.end())
        return;
    it->keys = keys;
    refreshShortcut(id);
}

void ActionRegistry::applyShortcuts(const ShortcutSettings& shortcuts)
{
    for (const QString& id : m_order)
        setKeys(id, shortcuts.keys(id));
}

void ActionRegistry::applyIcons(const IconSet& icons)
{
    for (const QString& id : m_order) {
        const Entry& e = m_actions[id];
        e.action->setIcon(icons.icon(e.iconName));
    }
}

void ActionRegistry::setActiveEditorScope(const QString& editorScope)
{
    if (m_activeEditorScope == editorScope)
        return;
    m_activeEditorScope = editorScope;
    for (const QString& id : m_order) {
        if (isEditorScope(m_actions[id].scope))
            refreshShortcut(id);
    }
}

bool ActionRegistry::isScopeActive(const QString& scope) const
{
    if (scope.isEmpty() || isWindowScope(scope))
        return true;
    return scope == m_activeEditorScope;
}

void ActionRegistry::refreshShortcut(const QString& id)
{
    const Entry& e = m_actions[id];
    e.action->setShortcut(isScopeActive(e.scope) ? e.keys : QKeySequence());
}

QStringList ActionRegistry::conflicts() const
{
    QMap<QString, QStringList> byKeys;
    for (const QString& id : m_order) {
        const Entry& e = m_actions[id];
        if (!e.keys.isEmpty())
            byKeys[e.keys.toString(QKeySequence::PortableText)] << id;
    }
    QStringList result;
    for (const QStringList& ids : std::as_const(byKeys)) {
        for (qsizetype i = 0; i < ids.size(); ++i) {
            for (qsizetype j = i + 1; j < ids.size(); ++j) {
                const QString& a = m_actions[ids[i]].scope;
                const QString& b = m_actions[ids[j]].scope;
                if (isWindowScope(a) != isWindowScope(b))
                    continue;
                const bool clash = a == b || a.isEmpty() || b.isEmpty();
                if (clash) {
                    if (!result.contains(ids[i]))
                        result << ids[i];
                    if (!result.contains(ids[j]))
                        result << ids[j];
                }
            }
        }
    }
    return result;
}

} // namespace OFS
