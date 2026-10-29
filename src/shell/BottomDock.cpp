#include "shell/BottomDock.h"

#include "shell/AdvancedSearchPanel.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QTabBar>
#include <QTableView>
#include <QTextEdit>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>

namespace OFS {

BottomDock::BottomDock(QWidget* parent)
    : QWidget(parent)
    , m_tabs(new QTabBar(this))
    , m_arrow(new QToolButton(this))
    , m_pages(new QStackedWidget(this))
{
    setObjectName(QStringLiteral("bottomDock"));

    m_tabs->setObjectName(QStringLiteral("bottomTabs"));
    m_tabs->setExpanding(false);
    m_tabs->setDrawBase(false);
    m_tabs->addTab(tr("Issues"));
    m_tabs->addTab(tr("Advanced Search"));
    m_tabs->addTab(tr("Engine Output"));
    m_tabs->addTab(tr("Debugger"));
    m_tabs->addTab(tr("Watch"));
    setIssueCounts(0, 0);

    m_arrow->setObjectName(QStringLiteral("bottomArrow"));
    m_arrow->setAutoRaise(true);
    connect(m_arrow, &QToolButton::clicked, this, [this] { setExpanded(!m_expanded); });

    // Issues: message (with severity icon) | file | line
    m_issues = new QTableView(this);
    m_issues->setObjectName(QStringLiteral("issuesView"));
    auto* issuesModel = new QStandardItemModel(0, 3, m_issues);
    issuesModel->setHorizontalHeaderLabels({tr("Description"), tr("File"), tr("Line")});
    m_issues->setModel(issuesModel);
    m_issues->horizontalHeader()->setStretchLastSection(false);
    m_issues->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_issues->verticalHeader()->hide();
    m_issues->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_issues->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_issues->setShowGrid(false);

    m_engineOutput = new QTextEdit(this);
    m_engineOutput->setObjectName(QStringLiteral("engineOutput"));
    m_engineOutput->setReadOnly(true);

    m_pages->addWidget(m_issues);
    m_advancedSearch = new AdvancedSearchPanel(this);
    m_pages->addWidget(m_advancedSearch);
    m_pages->addWidget(m_engineOutput);
    m_pages->addWidget(createTwoTreesPage(
        {tr("Level"), tr("Function"), tr("File"), tr("Line")},
        {tr("Number"), tr("Function"), tr("File"), tr("Line"), tr("Address"), tr("Condition"), tr("Ignore")},
        {QStringLiteral("callStack"), QStringLiteral("breakpoints")}));
    m_pages->addWidget(createTwoTreesPage({tr("Name"), tr("Value"), tr("Type")}, {tr("Name"), tr("Value"), tr("Type")},
                                          {QStringLiteral("watch1"), QStringLiteral("watch2")}));
    m_pages->hide();

    auto* bar = new QHBoxLayout;
    bar->setContentsMargins(0, 0, 0, 0);
    bar->setSpacing(0);
    bar->addWidget(m_tabs);
    bar->addStretch(1);
    bar->addWidget(m_arrow);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addLayout(bar);
    layout->addWidget(m_pages, 1);

    connect(m_tabs, &QTabBar::tabBarClicked, this, &BottomDock::onTabClicked);
    connect(m_tabs, &QTabBar::currentChanged, m_pages, &QStackedWidget::setCurrentIndex);
    updateArrow();
}

QWidget* BottomDock::createTwoTreesPage(const QStringList& left, const QStringList& right, const QStringList& names)
{
    auto* page = new QWidget(this);
    auto* layout = new QHBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    int i = 0;
    for (const QStringList& headers : {left, right}) {
        auto* tree = new QTreeView(page);
        tree->setObjectName(names.value(i++));
        auto* model = new QStandardItemModel(0, int(headers.size()), tree);
        model->setHorizontalHeaderLabels(headers);
        tree->setModel(model);
        tree->setRootIsDecorated(false);
        tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
        tree->setSelectionBehavior(QAbstractItemView::SelectRows);
        tree->setContextMenuPolicy(Qt::CustomContextMenu);
        layout->addWidget(tree);
        m_trees << tree;
    }
    return page;
}

void BottomDock::setExpanded(bool expanded)
{
    if (m_expanded == expanded)
        return;
    m_expanded = expanded;
    m_pages->setVisible(expanded);
    updateArrow();
    Q_EMIT expandedChanged(expanded);
}

int BottomDock::currentPage() const
{
    return m_tabs->currentIndex();
}

void BottomDock::setIssueCounts(int errors, int warnings)
{
    // Warnings are only counted when there are no errors, same as Fighter Factory.
    QString tip = tr("No issues found!");
    if (errors > 0)
        tip = tr("%1 error(s) found!").arg(errors);
    else if (warnings > 0)
        tip = tr("%1 warning(s) found!").arg(warnings);
    m_tabs->setTabToolTip(Issues, tip);
}

QString BottomDock::issuesToolTip() const
{
    return m_tabs->tabToolTip(Issues);
}

void BottomDock::showPage(int page)
{
    m_tabs->setCurrentIndex(page);
    setExpanded(true);
}

void BottomDock::onTabClicked(int index)
{
    if (index < 0)
        return;
    if (index != m_tabs->currentIndex())
        setExpanded(true);
    else
        setExpanded(!m_expanded);
}

void BottomDock::setArrowIcons(const QIcon& up, const QIcon& down)
{
    m_up = up;
    m_down = down;
    updateArrow();
}

void BottomDock::updateArrow()
{
    const QIcon& icon = m_expanded ? m_down : m_up;
    if (icon.isNull()) {
        m_arrow->setIcon(QIcon());
        m_arrow->setText(m_expanded ? QStringLiteral("⇩") : QStringLiteral("⇧"));
    } else {
        m_arrow->setIcon(icon);
        m_arrow->setText(QString());
    }
    m_arrow->setToolTip(m_expanded ? tr("Collapse") : tr("Expand"));
}

} // namespace OFS
