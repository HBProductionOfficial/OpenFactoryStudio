#include "shell/dialogs/ColorsPage.h"

#include "core/theme/ColorRoles.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QHBoxLayout>
#include <QListView>
#include <QPushButton>
#include <QStandardItemModel>
#include <QVBoxLayout>

namespace OFS {

namespace {

constexpr int kRoleKey = Qt::UserRole + 1;

} // namespace

ColorsPage::ColorsPage(QWidget* parent)
    : QWidget(parent)
    , m_model(new QStandardItemModel(this))
    , m_list(new QListView(this))
    , m_bold(new QCheckBox(tr("Bold"), this))
    , m_italic(new QCheckBox(tr("Italic"), this))
    , m_color(new QPushButton(tr("Color"), this))
    , m_save(new QPushButton(tr("Save"), this))
{
    m_list->setObjectName(QStringLiteral("colors"));
    m_bold->setObjectName(QStringLiteral("bold"));
    m_italic->setObjectName(QStringLiteral("italic"));
    m_color->setObjectName(QStringLiteral("setColor"));
    m_save->setObjectName(QStringLiteral("colorSave"));
    for (QPushButton* button : {m_color, m_save})
        button->setAutoDefault(false);

    for (const ColorRole& role : ColorRole::all()) {
        auto* item = new QStandardItem(role.displayName());
        item->setData(role.key, kRoleKey);
        item->setEditable(false);
        m_model->appendRow(item);
    }
    m_list->setModel(m_model);
    m_list->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setUniformItemSizes(true);

    auto* side = new QVBoxLayout;
    side->addWidget(m_bold);
    side->addWidget(m_italic);
    side->addWidget(m_color);
    side->addStretch(1);
    side->addWidget(m_save);

    auto* layout = new QHBoxLayout(this);
    layout->addWidget(m_list, 1);
    layout->addLayout(side);

    connect(m_list->selectionModel(), &QItemSelectionModel::currentChanged, this, &ColorsPage::updateControls);
    connect(m_list, &QListView::doubleClicked, this, &ColorsPage::chooseColor);
    connect(m_color, &QPushButton::clicked, this, &ColorsPage::chooseColor);
    connect(m_bold, &QCheckBox::toggled, this, &ColorsPage::styleToggled);
    connect(m_italic, &QCheckBox::toggled, this, &ColorsPage::styleToggled);
    connect(m_save, &QPushButton::clicked, this, &ColorsPage::saveRequested);

    setColorSet(ColorSet());
}

void ColorsPage::setColorSet(const ColorSet& set)
{
    m_set = ColorRole::complete(set);
    m_set.setName(set.name());
    m_modified = false;
    refreshRows();
    updateControls();
}

QString ColorsPage::currentRole() const
{
    return m_list->currentIndex().data(kRoleKey).toString();
}

void ColorsPage::setCurrentRole(const QString& role)
{
    for (int row = 0; row < m_model->rowCount(); ++row) {
        if (m_model->item(row)->data(kRoleKey).toString() == role) {
            m_list->setCurrentIndex(m_model->index(row, 0));
            return;
        }
    }
}

void ColorsPage::setRoleColor(const QString& role, const QColor& color)
{
    if (!ColorRole::find(role) || !color.isValid())
        return;
    ColorSet::Entry entry = m_set.entry(role);
    if (entry.color == color)
        return;
    entry.color = color;
    m_set.set(role, entry);
    m_modified = true;
    refreshRows();
    Q_EMIT edited();
}

void ColorsPage::setRoleStyle(const QString& role, bool bold, bool italic)
{
    const ColorRole* info = ColorRole::find(role);
    if (!info || !info->text)
        return;
    ColorSet::Entry entry = m_set.entry(role);
    if (entry.bold == bold && entry.italic == italic)
        return;
    entry.bold = bold;
    entry.italic = italic;
    m_set.set(role, entry);
    m_modified = true;
    refreshRows();
    updateControls();
    Q_EMIT edited();
}

void ColorsPage::refreshRows()
{
    // Text roles show as text on the editor background, the others as a filled row.
    const QColor background = m_set.color(QStringLiteral("background"));
    const QColor foreground = m_set.color(QStringLiteral("foreground"));
    for (int row = 0; row < m_model->rowCount(); ++row) {
        QStandardItem* item = m_model->item(row);
        const QString key = item->data(kRoleKey).toString();
        const ColorRole* role = ColorRole::find(key);
        const ColorSet::Entry entry = m_set.entry(key);
        QFont font = m_list->font();
        if (role && role->text) {
            item->setForeground(entry.color);
            item->setBackground(background);
            font.setBold(entry.bold);
            font.setItalic(entry.italic);
        } else {
            item->setForeground(foreground);
            item->setBackground(entry.color);
        }
        item->setFont(font);
    }
}

void ColorsPage::updateControls()
{
    const ColorRole* role = ColorRole::find(currentRole());
    const bool text = role && role->text;
    m_updating = true;
    const ColorSet::Entry entry = role ? m_set.entry(role->key) : ColorSet::Entry();
    m_bold->setChecked(text && entry.bold);
    m_italic->setChecked(text && entry.italic);
    m_updating = false;
    m_bold->setEnabled(text);
    m_italic->setEnabled(text);
    m_color->setEnabled(role != nullptr);
}

void ColorsPage::chooseColor()
{
    const ColorRole* role = ColorRole::find(currentRole());
    if (!role)
        return;
    const QColor color = QColorDialog::getColor(m_set.color(role->key), this, role->displayName());
    if (color.isValid())
        setRoleColor(role->key, color);
}

void ColorsPage::styleToggled()
{
    if (m_updating)
        return;
    setRoleStyle(currentRole(), m_bold->isChecked(), m_italic->isChecked());
}

} // namespace OFS
