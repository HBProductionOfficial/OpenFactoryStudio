#include "shell/dialogs/SpriteDialogs.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace OFS {

namespace {

QSpinBox* spinBox(const char* name, int minimum, int maximum, int value, QWidget* parent)
{
    auto* s = new QSpinBox(parent);
    s->setObjectName(QLatin1String(name));
    s->setRange(minimum, maximum);
    s->setValue(value);
    return s;
}

QRadioButton* radio(const QString& text, const char* name, QWidget* parent)
{
    auto* r = new QRadioButton(text, parent);
    r->setObjectName(QLatin1String(name));
    return r;
}

QDialogButtonBox* okCancel(QDialog* dialog, Qt::Orientation orientation = Qt::Horizontal)
{
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, orientation, dialog);
    buttons->setObjectName(QStringLiteral("buttonBox"));
    QObject::connect(buttons, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    return buttons;
}

void fillSpriteList(QListWidget* list, const SpriteDocument& doc, int current)
{
    list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    for (int i = 0; i < doc.spriteCount(); ++i)
        list->addItem(spriteCaption(doc.sprites()[i]));
    if (current >= 0 && current < list->count()) {
        list->item(current)->setSelected(true);
        list->setCurrentRow(current, QItemSelectionModel::NoUpdate);
        list->scrollToItem(list->item(current));
    }
}

QList<int> selectedRows(const QListWidget* list)
{
    QList<int> rows;
    for (int i = 0; i < list->count(); ++i) {
        if (list->item(i)->isSelected())
            rows << i;
    }
    return rows;
}

} // namespace

QString spriteCaption(const SpriteEntry& entry)
{
    QString caption = QStringLiteral("%1, %2").arg(entry.group()).arg(entry.number());
    if (!entry.name.isEmpty())
        caption += QStringLiteral(" - ") + entry.name;
    return caption;
}

SpriteRange::SpriteRange(QDialog* dialog, int currentGroup)
    : QObject(dialog)
    , m_widget(new QWidget(dialog))
{
    m_current = radio(tr("Current"), "cur", m_widget);
    m_current->setChecked(true);
    m_all = radio(tr("All"), "all", m_widget);
    m_group = radio(tr("Group"), "grp", m_widget);
    auto* box = new QGroupBox(m_widget);
    box->setObjectName(QStringLiteral("groupBox"));
    m_groupNumber = spinBox("grpNum", 0, 65535, currentGroup, box);
    m_interval = new QCheckBox(tr("Interval"), box);
    m_interval->setObjectName(QStringLiteral("interval"));
    m_start = spinBox("start", 0, 65535, 0, box);
    m_end = spinBox("end", 0, 65535, 0, box);
    auto* boxLayout = new QGridLayout(box);
    boxLayout->addWidget(m_groupNumber, 0, 0, 1, 2);
    boxLayout->addWidget(m_interval, 1, 0, 1, 2);
    boxLayout->addWidget(new QLabel(tr("Start:"), box), 2, 0);
    boxLayout->addWidget(m_start, 2, 1);
    boxLayout->addWidget(new QLabel(tr("End:"), box), 3, 0);
    boxLayout->addWidget(m_end, 3, 1);
    auto* layout = new QVBoxLayout(m_widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_current);
    layout->addWidget(m_all);
    layout->addWidget(m_group);
    layout->addWidget(box);
}

QList<int> SpriteRange::sprites(const SpriteDocument& doc, int current) const
{
    QList<int> result;
    if (m_current->isChecked()) {
        if (current >= 0 && current < doc.spriteCount())
            result << current;
        return result;
    }
    for (int i = 0; i < doc.spriteCount(); ++i) {
        const SpriteEntry& entry = doc.sprites()[i];
        if (m_group->isChecked()) {
            if (entry.group() != m_groupNumber->value())
                continue;
            if (m_interval->isChecked() && (entry.number() < m_start->value() || entry.number() > m_end->value()))
                continue;
        }
        result << i;
    }
    return result;
}

RangeDialog::RangeDialog(int currentGroup, QWidget* parent)
    : QDialog(parent)
    , m_range(new SpriteRange(this, currentGroup))
{
    setObjectName(QStringLiteral("SaveItem"));
    setWindowTitle(tr("Range"));
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(m_range->widget());
    layout->addWidget(okCancel(this));
    resize(217, 291);
}

ResizeDialog::ResizeDialog(int currentGroup, QWidget* parent)
    : QDialog(parent)
    , m_range(new SpriteRange(this, currentGroup))
{
    setObjectName(QStringLiteral("ResizeSpr"));
    setWindowTitle(tr("Resize"));
    auto scale = [this](const char* name) {
        auto* s = new QDoubleSpinBox(this);
        s->setObjectName(QLatin1String(name));
        s->setDecimals(2);
        s->setRange(0.25, 4.0);
        s->setSingleStep(0.25);
        s->setValue(1.0);
        return s;
    };
    m_width = scale("xscale");
    m_height = scale("yscale");
    m_filter = new QComboBox(this);
    m_filter->setObjectName(QStringLiteral("filter"));
    m_filter->addItems({tr("Nearest Neighbor"), tr("Bilinear"), tr("EPX + Nearest Neighbor"), tr("EPX + Bilinear"),
                        tr("HQ + Nearest Neighbor"), tr("HQ + Bilinear")});
    auto* form = new QGridLayout;
    form->addWidget(new QLabel(tr("Width scale:"), this), 0, 0);
    form->addWidget(m_width, 0, 1);
    form->addWidget(new QLabel(tr("Height scale:"), this), 1, 0);
    form->addWidget(m_height, 1, 1);
    form->addWidget(new QLabel(tr("Filter:"), this), 2, 0);
    form->addWidget(m_filter, 2, 1);
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(m_range->widget());
    layout->addLayout(form);
    layout->addWidget(okCancel(this));
    resize(231, 399);
}

double ResizeDialog::widthScale() const
{
    return m_width->value();
}

double ResizeDialog::heightScale() const
{
    return m_height->value();
}

Formats::ResizeFilter ResizeDialog::filter() const
{
    return Formats::ResizeFilter(std::clamp(m_filter->currentIndex(), 0, 5));
}

GroupDialog::GroupDialog(Kind kind, int currentGroup, QWidget* parent)
    : QDialog(parent)
{
    const bool rename = kind == Kind::Rename;
    setObjectName(rename ? QStringLiteral("RenGrp") : QStringLiteral("DelGrp"));
    setWindowTitle(rename ? tr("Rename group") : tr("Delete group"));
    m_group = spinBox("Grp", 0, 65535, currentGroup, this);
    m_entire = radio(tr("Entire group"), "AllGroup", this);
    m_entire->setChecked(true);
    QRadioButton* interval = radio(tr("Interval"), "Intervall", this);
    m_start = spinBox("Start", 0, 65535, 0, this);
    m_end = spinBox("End", 0, 65535, 0, this);
    auto* form = new QGridLayout;
    form->addWidget(new QLabel(tr("Group:"), this), 0, 0);
    form->addWidget(m_group, 0, 1);
    form->addWidget(m_entire, 1, 0, 1, 2);
    form->addWidget(interval, 2, 0, 1, 2);
    form->addWidget(new QLabel(tr("Start:"), this), 3, 0);
    form->addWidget(m_start, 3, 1);
    form->addWidget(new QLabel(tr("End:"), this), 4, 0);
    form->addWidget(m_end, 4, 1);
    if (rename) {
        m_newGroup = spinBox("Grp_2", 0, 65535, 0, this);
        form->addWidget(new QLabel(tr("New group:"), this), 5, 0);
        form->addWidget(m_newGroup, 5, 1);
    }
    auto* layout = new QHBoxLayout(this);
    layout->addLayout(form, 1);
    layout->addWidget(okCancel(this, Qt::Vertical));
    resize(rename ? 273 : 261, 211);
}

int GroupDialog::group() const
{
    return m_group->value();
}

int GroupDialog::first() const
{
    return m_entire->isChecked() ? -1 : m_start->value();
}

int GroupDialog::last() const
{
    return m_entire->isChecked() ? -1 : m_end->value();
}

int GroupDialog::newGroup() const
{
    return m_newGroup ? m_newGroup->value() : group();
}

AlignmentDialog::AlignmentDialog(const SpriteDocument& doc, int current, QWidget* parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("SprAlign"));
    setWindowTitle(tr("Alignment tool"));
    m_x = spinBox("xVal", -30000, 30000, 0, this);
    m_y = spinBox("yVal", -30000, 30000, 0, this);
    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("list"));
    fillSpriteList(m_list, doc, current);
    auto button = [this](const QString& text, const char* name) {
        auto* b = new QPushButton(text, this);
        b->setObjectName(QLatin1String(name));
        return b;
    };
    QPushButton* apply = button(tr("Apply"), "pushButton");
    apply->setDefault(true);
    QPushButton* close = button(tr("Close"), "pushButton_2");
    QPushButton* all = button(tr("Select all"), "pushButton_5");
    QPushButton* none = button(tr("Clear selection"), "pushButton_4");
    QPushButton* inverse = button(tr("Inverse"), "pushButton_3");
    connect(apply, &QPushButton::clicked, this,
            [this] { Q_EMIT applyRequested(selectedRows(m_list), m_x->value(), m_y->value()); });
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    connect(all, &QPushButton::clicked, m_list, &QListWidget::selectAll);
    connect(none, &QPushButton::clicked, m_list, &QListWidget::clearSelection);
    connect(inverse, &QPushButton::clicked, this, [this] {
        for (int i = 0; i < m_list->count(); ++i)
            m_list->item(i)->setSelected(!m_list->item(i)->isSelected());
    });

    auto* top = new QHBoxLayout;
    top->addWidget(new QLabel(tr("X:"), this));
    top->addWidget(m_x);
    top->addWidget(new QLabel(tr("Y:"), this));
    top->addWidget(m_y);
    top->addStretch(1);
    top->addWidget(apply);
    top->addWidget(close);
    auto* side = new QVBoxLayout;
    side->addStretch(1);
    side->addWidget(all);
    side->addWidget(none);
    side->addWidget(inverse);
    auto* middle = new QHBoxLayout;
    middle->addWidget(m_list, 1);
    middle->addLayout(side);
    auto* layout = new QVBoxLayout(this);
    layout->addLayout(top);
    layout->addLayout(middle, 1);
    resize(427, 299);
}

ItemsDialog::ItemsDialog(const SpriteDocument& doc, int current, QWidget* parent)
    : QDialog(parent)
    , m_list(new QListWidget(this))
{
    setObjectName(QStringLiteral("ItemsDlg"));
    setWindowTitle(tr("Items"));
    m_list->setObjectName(QStringLiteral("list"));
    fillSpriteList(m_list, doc, current);
    auto button = [this](const QString& text, const char* name) {
        auto* b = new QPushButton(text, this);
        b->setObjectName(QLatin1String(name));
        b->setAutoDefault(false);
        return b;
    };
    QPushButton* clear = button(tr("Clear"), "clearBut");
    QPushButton* invert = button(tr("Invert selection"), "invBut");
    QPushButton* all = button(tr("Select all"), "allBut");
    connect(clear, &QPushButton::clicked, m_list, &QListWidget::clearSelection);
    connect(all, &QPushButton::clicked, m_list, &QListWidget::selectAll);
    connect(invert, &QPushButton::clicked, this, [this] {
        for (int i = 0; i < m_list->count(); ++i)
            m_list->item(i)->setSelected(!m_list->item(i)->isSelected());
    });
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(m_list, 1);
    layout->addWidget(clear);
    layout->addWidget(invert);
    layout->addWidget(all);
    layout->addWidget(okCancel(this));
    resize(293, 371);
}

QList<int> ItemsDialog::sprites() const
{
    return selectedRows(m_list);
}

SpriteFormatDialog::SpriteFormatDialog(Formats::SffVersion current, QWidget* parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("SaveSpr"));
    setWindowTitle(tr("Format"));
    m_beta = radio(tr("M.U.G.E.N. Beta"), "mugenBT", this);
    m_v10 = radio(tr("M.U.G.E.N. 1.0"), "mugen10", this);
    m_v11 = radio(tr("M.U.G.E.N. 1.1"), "mugen11", this);
    (current == Formats::SffVersion::V1   ? m_beta
     : current == Formats::SffVersion::V2 ? m_v10
                                          : m_v11)
        ->setChecked(true);
    auto* grid = new QGridLayout;
    grid->addWidget(new QLabel(tr("Choose the format to be saved:"), this), 0, 0, 1, 2);
    grid->addWidget(m_beta, 1, 0);
    grid->addWidget(new QLabel(tr("8 bits only, other formats will be converted to 8 bits"), this), 1, 1);
    grid->addWidget(m_v10, 2, 0);
    grid->addWidget(new QLabel(tr("5 / 8 bits, other formats will be converted to 8 bits"), this), 2, 1);
    grid->addWidget(m_v11, 3, 0);
    grid->addWidget(new QLabel(tr("5 / 8 / 24 / 32 bits, other formats will be converted to 24 bits"), this), 3, 1);
    grid->setColumnStretch(1, 1);
    auto* layout = new QVBoxLayout(this);
    layout->addLayout(grid);
    layout->addWidget(okCancel(this));
    resize(492, 259);
}

Formats::SffVersion SpriteFormatDialog::version() const
{
    if (m_beta->isChecked())
        return Formats::SffVersion::V1;
    if (m_v10->isChecked())
        return Formats::SffVersion::V2;
    return Formats::SffVersion::V201;
}

SpriteExportDialog::SpriteExportDialog(QWidget* parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("ExpDlg"));
    setWindowTitle(tr("Export"));
    m_all = radio(tr("All"), "all", this);
    m_all->setChecked(true);
    QRadioButton* group = radio(tr("Entire group"), "allgrp", this);
    m_group = spinBox("grp", 0, 65535, 0, this);
    auto* form = new QGridLayout;
    form->addWidget(m_all, 0, 0, 1, 2);
    form->addWidget(group, 1, 0);
    form->addWidget(m_group, 1, 1);
    auto* layout = new QHBoxLayout(this);
    layout->addLayout(form, 1);
    layout->addWidget(okCancel(this, Qt::Vertical));
    resize(237, 137);
}

int SpriteExportDialog::group() const
{
    return m_all->isChecked() ? -1 : m_group->value();
}

} // namespace OFS
