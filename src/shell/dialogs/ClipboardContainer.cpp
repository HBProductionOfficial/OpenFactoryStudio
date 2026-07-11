#include "shell/dialogs/ClipboardContainer.h"

#include "core/actions/ActionRegistry.h"
#include "core/app/AppContext.h"
#include "core/tools/ClipboardStore.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QGridLayout>
#include <QListView>
#include <QMenu>
#include <QMimeData>
#include <QPushButton>

namespace OFS {

namespace {

QPointer<ClipboardContainer> g_container;

} // namespace

ClipboardContainer::ClipboardContainer(QWidget* parent)
    : QDialog(parent)
    , m_store(new ClipboardStore(this))
    , m_list(new QListView(this))
    , m_paste(new QPushButton(tr("Paste"), this))
    , m_remove(new QPushButton(tr("Remove"), this))
{
    setObjectName(QStringLiteral("ClipDlg"));
    setWindowTitle(tr("Clipboard"));

    auto* cutButton = new QPushButton(tr("Cut"), this);
    auto* copyButton = new QPushButton(tr("Copy"), this);
    cutButton->setObjectName(QStringLiteral("bCut"));
    copyButton->setObjectName(QStringLiteral("bCopy"));
    m_paste->setObjectName(QStringLiteral("bPaste"));
    m_remove->setObjectName(QStringLiteral("bRemove"));
    for (QPushButton* button : {cutButton, copyButton, m_paste, m_remove})
        button->setAutoDefault(false);

    m_list->setObjectName(QStringLiteral("list"));
    m_list->setModel(m_store);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setEditTriggers(QAbstractItemView::EditKeyPressed | QAbstractItemView::SelectedClicked);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);

    auto* grid = new QGridLayout(this);
    grid->addWidget(cutButton, 0, 0);
    grid->addWidget(copyButton, 0, 1);
    grid->addWidget(m_paste, 1, 0);
    grid->addWidget(m_remove, 1, 1);
    grid->addWidget(m_list, 2, 0, 1, 2);
    grid->setRowStretch(2, 1);

    connect(cutButton, &QPushButton::clicked, this, &ClipboardContainer::cut);
    connect(copyButton, &QPushButton::clicked, this, &ClipboardContainer::copy);
    connect(m_paste, &QPushButton::clicked, this, &ClipboardContainer::paste);
    connect(m_remove, &QPushButton::clicked, this, &ClipboardContainer::removeSelected);
    connect(m_list, &QListView::doubleClicked, this, &ClipboardContainer::paste);
    connect(m_list, &QWidget::customContextMenuRequested, this, &ClipboardContainer::showContextMenu);
    connect(m_list->selectionModel(), &QItemSelectionModel::currentChanged, this, &ClipboardContainer::updateButtons);
    connect(m_store, &QAbstractItemModel::rowsRemoved, this, &ClipboardContainer::updateButtons);
    connect(m_store, &QAbstractItemModel::modelReset, this, &ClipboardContainer::updateButtons);

    updateButtons();
    resize(278, 280);
}

ClipboardContainer::~ClipboardContainer() = default;

ClipboardContainer* ClipboardContainer::showClipboard(QWidget* parent)
{
    if (!g_container) {
        g_container = new ClipboardContainer(parent);
        if (!parent)
            connect(qApp, &QCoreApplication::aboutToQuit, g_container.data(), &QObject::deleteLater);
    }
    g_container->show();
    g_container->raise();
    g_container->activateWindow();
    return g_container;
}

ClipboardContainer* ClipboardContainer::instance()
{
    return g_container;
}

void ClipboardContainer::setEditActions(QAction* cut, QAction* copy, QAction* paste)
{
    m_cutAction = cut;
    m_copyAction = copy;
    m_pasteAction = paste;
    m_customActions = true;
}

QAction* ClipboardContainer::editAction(const QPointer<QAction>& explicitAction, const char* id) const
{
    if (m_customActions)
        return explicitAction.data();
    AppContext* context = AppContext::instance();
    return context ? context->actions().action(QString::fromLatin1(id)) : nullptr;
}

int ClipboardContainer::addCurrentClipboard(const QString& name)
{
    const int row = m_store->add(QApplication::clipboard()->mimeData(), name);
    if (row >= 0)
        setCurrentRow(row);
    return row;
}

bool ClipboardContainer::putBack(int row)
{
    QMimeData* data = m_store->copy(row);
    if (!data)
        return false;
    QApplication::clipboard()->setMimeData(data);
    return true;
}

void ClipboardContainer::captureAfter(QAction* action)
{
    if (action && action->isEnabled())
        action->trigger();
    addCurrentClipboard();
}

void ClipboardContainer::cut()
{
    captureAfter(editAction(m_cutAction, "edit_cut"));
}

void ClipboardContainer::copy()
{
    captureAfter(editAction(m_copyAction, "edit_copy"));
}

void ClipboardContainer::paste()
{
    if (!putBack(currentRow()))
        return;
    QAction* action = editAction(m_pasteAction, "edit_paste");
    if (action && action->isEnabled())
        action->trigger();
}

void ClipboardContainer::removeSelected()
{
    m_store->remove(currentRow());
}

void ClipboardContainer::renameSelected()
{
    const QModelIndex index = m_list->currentIndex();
    if (index.isValid())
        m_list->edit(index);
}

void ClipboardContainer::clearAll()
{
    m_store->clear();
}

int ClipboardContainer::currentRow() const
{
    const QModelIndex index = m_list->currentIndex();
    return index.isValid() ? index.row() : -1;
}

void ClipboardContainer::setCurrentRow(int row)
{
    m_list->setCurrentIndex(m_store->index(row));
    updateButtons();
}

void ClipboardContainer::updateButtons()
{
    const bool selected = currentRow() >= 0;
    m_paste->setEnabled(selected);
    m_remove->setEnabled(selected);
}

void ClipboardContainer::showContextMenu(const QPoint& pos)
{
    const QModelIndex index = m_list->indexAt(pos);
    if (index.isValid())
        m_list->setCurrentIndex(index);
    const bool selected = index.isValid();
    QMenu menu(this);
    QAction* pasteAction = menu.addAction(tr("Paste"));
    QAction* putAction = menu.addAction(tr("Put on clipboard"));
    QAction* renameAction = menu.addAction(tr("Rename"));
    QAction* removeAction = menu.addAction(tr("Remove"));
    menu.addSeparator();
    QAction* clearAction = menu.addAction(tr("Clear"));
    for (QAction* action : {pasteAction, putAction, renameAction, removeAction})
        action->setEnabled(selected);
    clearAction->setEnabled(m_store->count() > 0);
    QAction* chosen = menu.exec(m_list->viewport()->mapToGlobal(pos));
    if (chosen == pasteAction)
        paste();
    else if (chosen == putAction)
        putBack(currentRow());
    else if (chosen == renameAction)
        renameSelected();
    else if (chosen == removeAction)
        removeSelected();
    else if (chosen == clearAction)
        clearAll();
}

void ClipboardContainer::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    Q_EMIT visibilityChanged(true);
}

void ClipboardContainer::hideEvent(QHideEvent* event)
{
    QDialog::hideEvent(event);
    Q_EMIT visibilityChanged(false);
}

} // namespace OFS
