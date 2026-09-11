#pragma once

#include <QObject>

class QAction;
class QTreeView;

namespace OFS {

class ActionRegistry;
class BottomDock;
class IconSet;

namespace Debug {
class BreakpointList;
}

// Debugger and Watch pages of the bottom dock: the breakpoint list with its context menu, and the watch list menus.
class DebuggerPanels : public QObject
{
    Q_OBJECT

public:
    DebuggerPanels(BottomDock& dock, Debug::BreakpointList& breakpoints, ActionRegistry& actions, const IconSet& icons,
                   QObject* parent = nullptr);

    int selectedBreakpoint() const;
    void setIcons(const IconSet& icons);
    void setSessionActive(bool active);
    int watchList() const { return m_watchList; }

    // For tests: each row's cells joined by '|'.
    QStringList breakpointRows() const;

Q_SIGNALS:
    void editRequested(int number);

private:
    void refresh();
    void updateActions();
    void showBreakpointMenu(const QPoint& pos);
    void showWatchMenu(QTreeView* view, const QPoint& pos);

    BottomDock& m_dock;
    Debug::BreakpointList& m_breakpoints;
    ActionRegistry& m_actions;
    const IconSet* m_icons;
    bool m_sessionActive = false;
    int m_watchList = 0;
};

} // namespace OFS
