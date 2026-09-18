#include "shell/dialogs/RunChooserDialog.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

namespace OFS {

RunChooserDialog::RunChooserDialog(const QList<RunConfiguration>& configurations, int current, QWidget* parent)
    : QDialog(parent)
    , m_list(new QListWidget(this))
{
    setObjectName(QStringLiteral("RunDlg"));
    setWindowTitle(tr("Run in Engine"));
    auto* label = new QLabel(tr("Choose the target:"), this);
    label->setObjectName(QStringLiteral("label"));
    m_list->setObjectName(QStringLiteral("list"));
    m_list->setViewMode(QListView::IconMode);
    m_list->setMovement(QListView::Static);
    m_list->setResizeMode(QListView::Adjust);
    m_list->setWordWrap(true);
    m_list->setIconSize(QSize(48, 48));
    m_list->setGridSize(QSize(96, 88));
    for (const RunConfiguration& run : configurations) {
        m_list->addItem(new QListWidgetItem(run.displayIcon(), run.name));
    }
    m_list->setCurrentRow(qBound(0, current, int(configurations.size()) - 1));

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->setObjectName(QStringLiteral("buttonBox"));
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        if (m_list->currentRow() >= 0)
            accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_list, &QListWidget::itemDoubleClicked, this, &QDialog::accept);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(label);
    layout->addWidget(m_list, 1);
    layout->addWidget(buttons);
    resize(299, 264);
}

int RunChooserDialog::selectedIndex() const
{
    return m_list->currentRow();
}

} // namespace OFS
