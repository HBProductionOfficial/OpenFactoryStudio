#pragma once

#include <QWidget>

class QStackedWidget;
class QTabBar;
class QTableView;
class QTextEdit;
class QToolButton;

namespace OFS {

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

    void setArrowIcons(const QIcon& up, const QIcon& down);

Q_SIGNALS:
    void expandedChanged(bool expanded);

private:
    QWidget* createAdvancedSearchPage();
    QWidget* createTwoTreesPage(const QStringList& left, const QStringList& right);
    void onTabClicked(int index);
    void updateArrow();

    QTabBar* m_tabs;
    QToolButton* m_arrow;
    QStackedWidget* m_pages;
    QTableView* m_issues;
    QTextEdit* m_engineOutput;
    bool m_expanded = false;
    QIcon m_up;
    QIcon m_down;
};

} // namespace OFS
