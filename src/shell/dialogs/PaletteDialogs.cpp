#include "shell/dialogs/PaletteDialogs.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QListWidget>
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

QString paletteFileFilters()
{
    return QStringLiteral(
        "All supported (*.act *.pcx *.png *.gif *.bmp *.pal *.aco *.ase *.txt);;Adobe Color Table (*.act);;"
        "Personal Computer eXchange (*.pcx);;Portable Network Graphic (*.png);;Graphics Interchange Format (*.gif);;"
        "Bitmap (*.bmp);;RIFF / Jasc Palette (*.pal);;Adobe Color Swatch (*.aco);;Adobe Swatch Exchange (*.ase);;"
        "Corel Painter Swatches (*.txt)");
}

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

CommitSpriteDialog::CommitSpriteDialog(const QStringList& palettes, QWidget* parent)
    : QDialog(parent)
    , m_reversed(new QCheckBox(tr("Reversed"), this))
    , m_mode(new QComboBox(this))
    , m_list(new QListWidget(this))
{
    setObjectName(QStringLiteral("PalDlg"));
    setWindowTitle(tr("Palette"));
    m_reversed->setObjectName(QStringLiteral("reversed"));
    m_mode->setObjectName(QStringLiteral("pal"));
    m_mode->addItems({tr("Image's palette, if not already added"), tr("Force the specified palette (adapt the image)"),
                      tr("Exchange with the specified palette")});
    m_list->setObjectName(QStringLiteral("list"));
    m_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_list->addItems(palettes);
    if (m_list->count() > 0)
        m_list->item(0)->setSelected(true);
    auto button = [this](const QString& text, const char* name) {
        auto* b = new QPushButton(text, this);
        b->setObjectName(QLatin1String(name));
        b->setAutoDefault(false);
        return b;
    };
    QPushButton* clear = button(tr("Clear"), "clearBut");
    QPushButton* all = button(tr("Select all"), "allBut");
    QPushButton* invert = button(tr("Invert selection"), "invBut");
    connect(clear, &QPushButton::clicked, m_list, &QListWidget::clearSelection);
    connect(all, &QPushButton::clicked, m_list, &QListWidget::selectAll);
    connect(invert, &QPushButton::clicked, this, [this] {
        for (int i = 0; i < m_list->count(); ++i)
            m_list->item(i)->setSelected(!m_list->item(i)->isSelected());
    });
    auto* ok = new QPushButton(tr("OK"), this);
    ok->setObjectName(QStringLiteral("okBut"));
    ok->setDefault(true);
    auto* ignore = new QPushButton(tr("Ignore"), this);
    ignore->setObjectName(QStringLiteral("ignoreBut"));
    connect(ok, &QPushButton::clicked, this, &QDialog::accept);
    connect(ignore, &QPushButton::clicked, this, [this] { done(Ignored); });

    auto* selection = new QHBoxLayout;
    selection->addWidget(clear);
    selection->addWidget(all);
    selection->addWidget(invert);
    auto* buttons = new QHBoxLayout;
    buttons->addStretch(1);
    buttons->addWidget(ok);
    buttons->addWidget(ignore);
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(m_reversed);
    layout->addWidget(m_mode);
    layout->addWidget(m_list, 1);
    layout->addLayout(selection);
    layout->addLayout(buttons);
    resize(373, 365);
}

CommitSpriteDialog::Mode CommitSpriteDialog::mode() const
{
    return Mode(std::clamp(m_mode->currentIndex(), 0, 2));
}

bool CommitSpriteDialog::reversed() const
{
    return m_reversed->isChecked();
}

QList<int> CommitSpriteDialog::palettes() const
{
    QList<int> rows;
    for (int i = 0; i < m_list->count(); ++i) {
        if (m_list->item(i)->isSelected())
            rows << i;
    }
    return rows;
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
