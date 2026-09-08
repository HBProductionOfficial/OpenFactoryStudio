#pragma once

#include <QWidget>

class QStackedWidget;
class QTabBar;
class QTableView;
class QTextEdit;
class QToolButton;
class QTreeView;

namespace OFS {

class AdvancedSearchPanel;

// Tabbed area at the bottom of the workspace: Issues, Advanced Search, Engine Output, Debugger and Watch. Collapsed, it
// only shows the tab bar. The arrow button at the right toggles it.
class BottomDock : public QWidget
{
    Q_OBJECT

public:
    enum Page
    {
        Issues,
        AdvancedSearch,
        EngineOutput,
        Debugger,
        Watch,
    };

    explicit BottomDock(QWidget* parent = nullptr);

    bool isExpanded() const { return m_expanded; }
    void setExpanded(bool expanded);
    int currentPage() const;

    void showPage(int page);

    QTextEdit* engineOutput() const { return m_engineOutput; }
    QTableView* issuesView() const { return m_issues; }
    AdvancedSearchPanel* advancedSearch() const { return m_advancedSearch; }
    QTreeView* callStackView() const { return m_trees.value(0); }
    QTreeView* breakpointsView() const { return m_trees.value(1); }
    QTreeView* watchView(int list) const { return m_trees.value(2 + list); }

    void setArrowIcons(const QIcon& up, const QIcon& down);

Q_SIGNALS:
    void expandedChanged(bool expanded);

private:
    QWidget* createTwoTreesPage(const QStringList& left, const QStringList& right, const QStringList& names);
    void onTabClicked(int index);
    void updateArrow();

    QTabBar* m_tabs;
    QToolButton* m_arrow;
    QStackedWidget* m_pages;
    QTableView* m_issues;
    QTextEdit* m_engineOutput;
    AdvancedSearchPanel* m_advancedSearch;
    QList<QTreeView*> m_trees;
    bool m_expanded = false;
    QIcon m_up;
    QIcon m_down;
};

} // namespace OFS
