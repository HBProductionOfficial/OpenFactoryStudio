#include "shell/dialogs/ProjectDialogs.h"

#include "core/engine/Engine.h"
#include "core/engine/EngineManager.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QIcon>
#include <QListWidget>
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
            const QDir folder(root + QStringLiteral("/scripts/project"));
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
