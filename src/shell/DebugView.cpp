#include "shell/DebugView.h"

#include <QDir>
#include <QSet>
#include <QStandardItemModel>
#include <QTreeView>

namespace OFS::DebugView {

namespace {

constexpr int PathRole = Qt::UserRole + 1;

QStandardItemModel* modelOf(QTreeView* tree, const QStringList& headers)
{
    auto* model = qobject_cast<QStandardItemModel*>(tree->model());
    if (!model || model->columnCount() != headers.size()) {
        model = new QStandardItemModel(0, int(headers.size()), tree);
        tree->setModel(model);
    }
    model->setHorizontalHeaderLabels(headers);
    return model;
}

QList<QStandardItem*> rowOf(const Debug::Variable& v)
{
    auto* name = new QStandardItem(v.name);
    name->setData(v.path, PathRole);
    name->setEditable(false);
    auto* value = new QStandardItem(v.value);
    value->setEditable(false);
    auto* type = new QStandardItem(v.type);
    type->setEditable(false);
    for (const Debug::Variable& child : v.children)
        name->appendRow(rowOf(child));
    return {name, value, type};
}

// True when the rows are the same (by path, recursively), so the values can change in place.
bool sameShape(const QStandardItem* parent, int count, const QList<Debug::Variable>& list,
               const QStandardItemModel* model)
{
    if (count != list.size())
        return false;
    for (int i = 0; i < list.size(); ++i) {
        const QStandardItem* item = parent ? parent->child(i, 0) : model->item(i, 0);
        if (!item || item->data(PathRole).toString() != list[i].path ||
            !sameShape(item, item->rowCount(), list[i].children, model))
            return false;
    }
    return true;
}

void updateValues(QStandardItem* parent, const QList<Debug::Variable>& list, QStandardItemModel* model)
{
    for (int i = 0; i < list.size(); ++i) {
        QStandardItem* name = parent ? parent->child(i, 0) : model->item(i, 0);
        QStandardItem* value = parent ? parent->child(i, 1) : model->item(i, 1);
        if (value && value->text() != list[i].value)
            value->setText(list[i].value);
        if (name)
            updateValues(name, list[i].children, model);
    }
}

void expandedPaths(const QTreeView* tree, const QStandardItemModel* model, const QModelIndex& parent,
                   QSet<QString>& out)
{
    for (int i = 0; i < model->rowCount(parent); ++i) {
        const QModelIndex index = model->index(i, 0, parent);
        if (tree->isExpanded(index)) {
            out.insert(index.data(PathRole).toString());
            expandedPaths(tree, model, index, out);
        }
    }
}

void expand(QTreeView* tree, const QStandardItemModel* model, const QModelIndex& parent, const QSet<QString>& paths)
{
    for (int i = 0; i < model->rowCount(parent); ++i) {
        const QModelIndex index = model->index(i, 0, parent);
        if (paths.contains(index.data(PathRole).toString())) {
            tree->setExpanded(index, true);
            expand(tree, model, index, paths);
        }
    }
}

} // namespace

void showVariables(QTreeView* tree, const QList<Debug::Variable>& variables)
{
    if (!tree)
        return;
    QStandardItemModel* model = modelOf(tree, {QObject::tr("Name"), QObject::tr("Value"), QObject::tr("Type")});
    if (sameShape(nullptr, model->rowCount(), variables, model)) {
        updateValues(nullptr, variables, model);
        return;
    }
    QSet<QString> expanded;
    expandedPaths(tree, model, QModelIndex(), expanded);
    const QString current = currentPath(tree);
    model->removeRows(0, model->rowCount());
    for (const Debug::Variable& v : variables)
        model->appendRow(rowOf(v));
    expand(tree, model, QModelIndex(), expanded);
    if (!current.isEmpty()) {
        const QModelIndexList found =
            model->match(model->index(0, 0), PathRole, current, 1, Qt::MatchExactly | Qt::MatchRecursive);
        if (!found.isEmpty())
            tree->setCurrentIndex(found.first());
    }
}

void showWatch(QTreeView* tree, const QList<Debug::Variable>& rows)
{
    if (!tree)
        return;
    QStandardItemModel* model = modelOf(tree, {QObject::tr("Name"), QObject::tr("Value"), QObject::tr("Type")});
    if (sameShape(nullptr, model->rowCount(), rows, model)) {
        updateValues(nullptr, rows, model);
        return;
    }
    model->removeRows(0, model->rowCount());
    for (const Debug::Variable& v : rows) {
        Debug::Variable flat = v;
        flat.children.clear();
        model->appendRow(rowOf(flat));
    }
}

void showStack(QTreeView* tree, const QList<Debug::StackFrame>& frames)
{
    if (!tree)
        return;
    QStandardItemModel* model =
        modelOf(tree, {QObject::tr("Level"), QObject::tr("Function"), QObject::tr("File"), QObject::tr("Line")});
    model->removeRows(0, model->rowCount());
    for (int i = 0; i < frames.size(); ++i) {
        QList<QStandardItem*> row{new QStandardItem(QString::number(i)), new QStandardItem(frames[i].function),
                                  new QStandardItem(QDir::toNativeSeparators(frames[i].file)),
                                  new QStandardItem(QString::number(frames[i].line))};
        for (QStandardItem* item : row)
            item->setEditable(false);
        row.first()->setData(frames[i].file, PathRole);
        model->appendRow(row);
    }
}

QString currentPath(const QTreeView* tree)
{
    if (!tree || !tree->currentIndex().isValid())
        return QString();
    const QModelIndex index = tree->currentIndex().siblingAtColumn(0);
    if (tree->model()->rowCount(index) > 0)
        return QString();
    return index.data(PathRole).toString();
}

} // namespace OFS::DebugView
