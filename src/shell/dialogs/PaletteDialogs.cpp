#include "shell/dialogs/PaletteDialogs.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QVBoxLayout>

namespace OFS {

namespace {

class IdentifierDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem&, const QModelIndex&) const override
    {
        auto* spin = new QSpinBox(parent);
        spin->setRange(0, 65535);
        spin->setFrame(false);
        return spin;
    }
};

} // namespace

RemapDialog::RemapDialog(QWidget* parent)
    : QDialog(parent)
    , m_model(new QStandardItemModel(0, 4, this))
    , m_table(new QTableView(this))
{
    setObjectName(QStringLiteral("RemapDlg"));
    setWindowTitle(tr("Remap"));
    setModal(false);
    m_model->setHorizontalHeaderLabels({tr("Src Grp"), tr("Src Num"), tr("Dst Grp"), tr("Dst Num")});
    m_table->setObjectName(QStringLiteral("tableView"));
    m_table->setModel(m_model);
    m_table->setItemDelegate(new IdentifierDelegate(m_table));
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->verticalHeader()->hide();

    auto* add = new QPushButton(tr("Add"), this);
    add->setObjectName(QStringLiteral("pushButton"));
    add->setDefault(true);
    auto* remove = new QPushButton(tr("Delete"), this);
    remove->setObjectName(QStringLiteral("pushButton_2"));
    auto* clear = new QPushButton(tr("Clear"), this);
    clear->setObjectName(QStringLiteral("pushButton_3"));
    connect(add, &QPushButton::clicked, this, [this] {
        addRow(RemapRow());
        Q_EMIT rowsChanged();
    });
    connect(remove, &QPushButton::clicked, this, [this] {
        const QModelIndex index = m_table->currentIndex();
        if (!index.isValid())
            return;
        m_model->removeRow(index.row());
        Q_EMIT rowsChanged();
    });
    connect(clear, &QPushButton::clicked, this, [this] {
        m_model->removeRows(0, m_model->rowCount());
        Q_EMIT rowsChanged();
    });
    connect(m_model, &QStandardItemModel::itemChanged, this, &RemapDialog::rowsChanged);

    auto* buttons = new QHBoxLayout;
    buttons->addWidget(add);
    buttons->addWidget(remove);
    buttons->addStretch(1);
    buttons->addWidget(clear);
    auto* layout = new QVBoxLayout(this);
    layout->addLayout(buttons);
    layout->addWidget(m_table, 1);
    resize(415, 281);
    addRow(RemapRow());
}

void RemapDialog::addRow(const RemapRow& row)
{
    QList<QStandardItem*> items;
    for (int value : {row.sourceGroup, row.sourceNumber, row.targetGroup, row.targetNumber}) {
        auto* item = new QStandardItem;
        item->setData(value, Qt::EditRole);
        items << item;
    }
    m_model->appendRow(items); // (itemChanged is only emitted by later edits)
}

QList<RemapRow> RemapDialog::rows() const
{
    QList<RemapRow> rows;
    for (int r = 0; r < m_model->rowCount(); ++r) {
        auto value = [this, r](int column) {
            return m_model->item(r, column)->data(Qt::EditRole).toInt();
        };
        rows << RemapRow{value(0), value(1), value(2), value(3)};
    }
    return rows;
}

void RemapDialog::setRows(const QList<RemapRow>& rows)
{
    m_model->removeRows(0, m_model->rowCount());
    for (const RemapRow& row : rows)
        addRow(row);
    Q_EMIT rowsChanged();
}

} // namespace OFS
