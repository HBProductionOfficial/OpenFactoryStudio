#include "shell/dialogs/ProjectDialogs.h"

#include "core/engine/Engine.h"
#include "core/engine/EngineManager.h"
#include "core/script/ScriptFolders.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHash>
#include <QIcon>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QTabWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace OFS {

QList<ProjectTemplate> findProjectTemplates(const EngineManager& engines)
{
    QList<ProjectTemplate> templates;
    for (const Engine* engine : engines.engines()) {
        for (const QString& root : {engine->dataDir(), engine->userDataDir()}) {
            if (root.isEmpty())
                continue;
            const QDir folder(Script::scriptFolder(root, Script::ScriptType::Project));
            for (const QString& category : folder.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
                const QDir dir(folder.filePath(category));
                for (const QFileInfo& script : dir.entryInfoList({QStringLiteral("*.lua")}, QDir::Files, QDir::Name)) {
                    ProjectTemplate t;
                    t.category = category;
                    t.name = script.completeBaseName();
                    t.script = script.absoluteFilePath();
                    t.engine = engine->name();
                    for (const char* suffix : {".png", ".svg"}) {
                        const QString icon = dir.filePath(t.name + QLatin1String(suffix));
                        if (QFileInfo::exists(icon)) {
                            t.icon = icon;
                            break;
                        }
                    }
                    templates << t;
                }
            }
        }
    }
    // Categories keep the order they were found in, and the user's templates join theirs.
    QStringList order;
    for (const ProjectTemplate& t : std::as_const(templates)) {
        if (!order.contains(t.category))
            order << t.category;
    }
    std::stable_sort(templates.begin(), templates.end(), [&order](const ProjectTemplate& a, const ProjectTemplate& b) {
        return order.indexOf(a.category) < order.indexOf(b.category);
    });
    return templates;
}

ProjectTemplateDialog::ProjectTemplateDialog(const EngineManager& engines, QWidget* parent)
    : QDialog(parent)
    , m_tabs(new QTabWidget(this))
    , m_templates(findProjectTemplates(engines))
{
    setObjectName(QStringLiteral("ProjDlg"));
    setWindowTitle(tr("Project template"));
    m_tabs->setObjectName(QStringLiteral("tabs"));

    QHash<QString, QListWidget*> lists;
    for (int i = 0; i < m_templates.size(); ++i) {
        const ProjectTemplate& t = m_templates.at(i);
        QListWidget* list = lists.value(t.category);
        if (!list) {
            list = new QListWidget;
            list->setViewMode(QListView::IconMode);
            list->setIconSize(QSize(48, 48));
            list->setResizeMode(QListView::Adjust);
            list->setMovement(QListView::Static);
            list->setWordWrap(true);
            list->setSpacing(6);
            lists.insert(t.category, list);
            m_tabs->addTab(list, t.category);
            connect(list, &QListWidget::itemDoubleClicked, this, &QDialog::accept);
        }
        const Engine* engine = engines.engine(t.engine);
        const QString caption =
            QStringLiteral("%1 (%2)").arg(t.name, engine ? engine->displayName() : t.engine); // "Empty (MUGENext)"
        auto* item = new QListWidgetItem(t.icon.isEmpty() ? QIcon() : QIcon(t.icon), caption, list);
        item->setData(Qt::UserRole, i);
        item->setToolTip(QDir::toNativeSeparators(t.script));
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->setObjectName(QStringLiteral("buttonBox"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(m_tabs);
    layout->addWidget(buttons);
    resize(400, 300);
}

BackupDialog::BackupDialog(const QList<Backup::Item>& backups, QWidget* parent)
    : QDialog(parent)
    , m_list(new QListWidget(this))
    , m_backups(backups)
{
    setObjectName(QStringLiteral("BackupDlg"));
    setWindowTitle(tr("Restore backup"));
    auto* label = new QLabel(
        tr("Choose a backup to be restored:\nAttention: all files will be overwritten by the backup."), this);
    label->setObjectName(QStringLiteral("label"));
    m_list->setObjectName(QStringLiteral("listView"));
    for (const Backup::Item& backup : backups) {
        auto* item = new QListWidgetItem(backup.label, m_list);
        item->setToolTip(QDir::toNativeSeparators(backup.path));
    }
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->setObjectName(QStringLiteral("buttonBox"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_list, &QListWidget::itemDoubleClicked, this, &QDialog::accept);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(label);
    layout->addWidget(m_list, 1);
    layout->addWidget(buttons);
    resize(392, 288);
}

QString BackupDialog::selectedBackup() const
{
    const QListWidgetItem* item = m_list->currentItem();
    if (!item || !item->isSelected())
        return {};
    return m_backups.value(m_list->row(item)).path;
}

ItemImportDialog::ItemImportDialog(Kind kind, const QList<Source>& projects, QWidget* parent)
    : QDialog(parent)
    , m_projects(projects)
{
    setObjectName(QStringLiteral("ProjImportDlg"));
    setWindowTitle(tr("Import from another project"));
    auto button = [](const QString& text, const char* name, QWidget* parentWidget) {
        auto* b = new QPushButton(text, parentWidget);
        b->setObjectName(QLatin1String(name));
        b->setAutoDefault(false);
        return b;
    };
    auto radio = [](const QString& text, const char* name, QWidget* parentWidget) {
        auto* r = new QRadioButton(text, parentWidget);
        r->setObjectName(QLatin1String(name));
        return r;
    };

    auto* source = new QGroupBox(tr("Source:"), this);
    source->setObjectName(QStringLiteral("groupBox"));
    m_project = new QComboBox(source);
    m_project->setObjectName(QStringLiteral("projList"));
    for (const Source& project : projects)
        m_project->addItem(project.name);
    m_sorted = new QCheckBox(tr("Sort "), source);
    m_sorted->setObjectName(QStringLiteral("sorted"));
    m_source = new QListWidget(source);
    m_source->setObjectName(QStringLiteral("list"));
    m_source->setSelectionMode(QAbstractItemView::ExtendedSelection);
    QPushButton* all = button(tr("Select all"), "allBut", source);
    QPushButton* invert = button(tr("Invert selection"), "invBut", source);
    QPushButton* clear = button(tr("Clear"), "clearBut", source);
    auto* sourceLayout = new QGridLayout(source);
    auto* projectLabel = new QLabel(tr("Project:"), source);
    projectLabel->setObjectName(QStringLiteral("label"));
    sourceLayout->addWidget(projectLabel, 0, 0, 1, 3);
    sourceLayout->addWidget(m_project, 1, 0, 1, 2);
    sourceLayout->addWidget(m_sorted, 1, 2);
    sourceLayout->addWidget(m_source, 2, 0, 1, 3);
    sourceLayout->addWidget(all, 3, 0);
    sourceLayout->addWidget(invert, 3, 1);
    sourceLayout->addWidget(clear, 3, 2);

    auto* destination = new QGroupBox(tr("Destination:"), this);
    destination->setObjectName(QStringLiteral("groupBox_2"));
    m_destination = new QListWidget(destination);
    m_destination->setObjectName(QStringLiteral("destList"));
    m_destination->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_destination->setEditTriggers(QAbstractItemView::DoubleClicked);
    QPushButton* add = button(tr("Add from source"), "addBut", destination);
    QPushButton* remove = button(tr("Remove selected"), "delBut", destination);
    auto* hint = new QLabel(tr("Double-click an item to rename."), destination);
    hint->setObjectName(QStringLiteral("label_2"));
    auto* destinationLayout = new QGridLayout(destination);
    destinationLayout->addWidget(hint, 0, 0, 1, 2);
    destinationLayout->addWidget(m_destination, 1, 0, 1, 2);
    destinationLayout->addWidget(add, 2, 0);
    destinationLayout->addWidget(remove, 2, 1);

    auto* conflicts = new QGroupBox(tr("Conflicts:"), this);
    conflicts->setObjectName(QStringLiteral("groupBox_3"));
    m_ask = radio(tr("Ask when needed"), "ask", conflicts);
    m_ask->setChecked(true);
    m_replace = radio(tr("Replace existing"), "replace", conflicts);
    m_skip = radio(tr("Skip (don't add)"), "skip", conflicts);
    m_asIs = radio(tr("Add anyway (as is)"), "asIs", conflicts);
    auto* conflictLayout = new QVBoxLayout(conflicts);
    for (QRadioButton* r : {m_ask, m_replace, m_skip, m_asIs})
        conflictLayout->addWidget(r);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->setObjectName(QStringLiteral("buttonBox"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* right = new QVBoxLayout;
    right->addWidget(destination, 1);
    if (kind == Kind::Animations) {
        m_sprites = new QCheckBox(tr("Import sprites"), this);
        right->addWidget(m_sprites);
    }
    right->addWidget(conflicts);
    right->addWidget(buttons);
    auto* layout = new QHBoxLayout(this);
    layout->addWidget(source, 1);
    layout->addLayout(right, 1);
    resize(659, 445);

    connect(m_project, &QComboBox::currentIndexChanged, this, [this] {
        m_destination->clear();
        fillSource();
    });
    connect(m_sorted, &QCheckBox::toggled, this, &ItemImportDialog::fillSource);
    connect(all, &QPushButton::clicked, m_source, &QListWidget::selectAll);
    connect(clear, &QPushButton::clicked, m_source, &QListWidget::clearSelection);
    connect(invert, &QPushButton::clicked, this, [this] {
        for (int i = 0; i < m_source->count(); ++i)
            m_source->item(i)->setSelected(!m_source->item(i)->isSelected());
    });
    connect(add, &QPushButton::clicked, this, [this] {
        const int project = m_project->currentIndex();
        if (project < 0 || project >= m_projects.size())
            return;
        for (int row = 0; row < m_source->count(); ++row) {
            const QListWidgetItem* selected = m_source->item(row);
            if (!selected->isSelected())
                continue;
            const int index = selected->data(Qt::UserRole).toInt();
            auto* item = new QListWidgetItem(m_projects[project].items[index].identifier);
            item->setData(Qt::UserRole, index);
            item->setFlags(item->flags() | Qt::ItemIsEditable);
            m_destination->addItem(item);
        }
    });
    connect(remove, &QPushButton::clicked, this, [this] { qDeleteAll(m_destination->selectedItems()); });
    fillSource();
}

void ItemImportDialog::fillSource()
{
    m_source->clear();
    const int project = m_project->currentIndex();
    if (project < 0 || project >= m_projects.size())
        return;
    const QList<Item>& items = m_projects[project].items;
    QList<int> order;
    for (int i = 0; i < items.size(); ++i)
        order << i;
    if (m_sorted->isChecked())
        std::stable_sort(order.begin(), order.end(),
                         [&items](int a, int b) { return items[a].number < items[b].number; });
    for (int index : std::as_const(order)) {
        auto* item = new QListWidgetItem(items[index].caption, m_source);
        item->setData(Qt::UserRole, index);
    }
}

int ItemImportDialog::project() const
{
    return m_project->currentIndex();
}

QList<ItemImportDialog::Chosen> ItemImportDialog::items() const
{
    QList<Chosen> chosen;
    for (int row = 0; row < m_destination->count(); ++row) {
        const QListWidgetItem* item = m_destination->item(row);
        chosen << Chosen{item->data(Qt::UserRole).toInt(), item->text().trimmed()};
    }
    return chosen;
}

ItemImportDialog::Conflict ItemImportDialog::conflict() const
{
    if (m_replace->isChecked())
        return Conflict::Replace;
    if (m_skip->isChecked())
        return Conflict::Skip;
    if (m_asIs->isChecked())
        return Conflict::AsIs;
    return Conflict::Ask;
}

bool ItemImportDialog::importSprites() const
{
    return m_sprites && m_sprites->isChecked();
}

ProjectTemplate ProjectTemplateDialog::selectedTemplate() const
{
    // Nothing is selected when the window opens, so OK alone chooses nothing.
    const auto* list = qobject_cast<const QListWidget*>(m_tabs->currentWidget());
    const QListWidgetItem* item = list ? list->currentItem() : nullptr;
    if (!item || !item->isSelected())
        return {};
    return m_templates.value(item->data(Qt::UserRole).toInt());
}

} // namespace OFS
