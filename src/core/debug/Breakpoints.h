#pragma once

#include "core/CoreExport.h"

#include <QList>
#include <QObject>
#include <QString>

namespace OFS::Debug {

enum class BreakpointType
{
    FileLine,
    Function, // before a function runs
    Memory,   // after the data at an address changes
    Frame,
};

struct OFS_CORE_EXPORT Breakpoint
{
    int number = 0;
    BreakpointType type = BreakpointType::FileLine;
    QString file; // absolute, with '/'
    int line = 0; // from 1
    QString function;
    QString address;
    bool enabled = true;
    QString condition;
    int ignoreCount = 0; // passes before it stops
    bool oneShot = false;

    bool isValid() const;
};

// Numbered from 1 in the order added. Numbering starts again once they are all removed.
class OFS_CORE_EXPORT BreakpointList : public QObject
{
    Q_OBJECT

public:
    explicit BreakpointList(QObject* parent = nullptr);

    const QList<Breakpoint>& breakpoints() const { return m_list; }
    bool isEmpty() const { return m_list.isEmpty(); }

    int add(Breakpoint breakpoint);
    bool update(const Breakpoint& breakpoint);
    bool remove(int number);
    void removeAll();
    bool setEnabled(int number, bool enabled);
    const Breakpoint* find(int number) const;
    const Breakpoint* at(const QString& file, int line) const;
    bool toggle(const QString& file, int line);
    QList<Breakpoint> inFile(const QString& file) const;
    void renameFile(const QString& from, const QString& to);

Q_SIGNALS:
    void changed();

private:
    QList<Breakpoint> m_list;
    int m_next = 1;
};

} // namespace OFS::Debug
