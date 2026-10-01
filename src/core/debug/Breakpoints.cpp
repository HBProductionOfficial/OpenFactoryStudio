#include "core/debug/Breakpoints.h"

#include <QDir>

namespace OFS::Debug {

namespace {

QString normalized(const QString& file)
{
    QString path = file;
    path.replace(QLatin1Char('\\'), QLatin1Char('/'));
    return QDir::cleanPath(path);
}

bool samePath(const QString& a, const QString& b)
{
    return normalized(a).compare(normalized(b), Qt::CaseInsensitive) == 0;
}

} // namespace

bool Breakpoint::isValid() const
{
    switch (type) {
    case BreakpointType::FileLine:
        return !file.trimmed().isEmpty() && line > 0;
    case BreakpointType::Function:
        return !function.trimmed().isEmpty();
    case BreakpointType::Memory:
        return !address.trimmed().isEmpty();
    case BreakpointType::Frame:
        return true;
    }
    return false;
}

BreakpointList::BreakpointList(QObject* parent)
    : QObject(parent)
{}

int BreakpointList::add(Breakpoint breakpoint)
{
    if (!breakpoint.isValid())
        return 0;
    if (breakpoint.type == BreakpointType::FileLine)
        breakpoint.file = normalized(breakpoint.file);
    breakpoint.number = m_next++;
    m_list << breakpoint;
    Q_EMIT changed();
    return breakpoint.number;
}

bool BreakpointList::update(const Breakpoint& breakpoint)
{
    for (Breakpoint& b : m_list) {
        if (b.number != breakpoint.number)
            continue;
        Breakpoint changedOne = breakpoint;
        changedOne.type = b.type;
        if (!changedOne.isValid())
            return false;
        if (changedOne.type == BreakpointType::FileLine)
            changedOne.file = normalized(changedOne.file);
        b = changedOne;
        Q_EMIT changed();
        return true;
    }
    return false;
}

bool BreakpointList::remove(int number)
{
    for (int i = 0; i < m_list.size(); ++i) {
        if (m_list[i].number == number) {
            m_list.removeAt(i);
            Q_EMIT changed();
            return true;
        }
    }
    return false;
}

void BreakpointList::removeAll()
{
    m_list.clear();
    m_next = 1;
    Q_EMIT changed();
}

bool BreakpointList::setEnabled(int number, bool enabled)
{
    for (Breakpoint& b : m_list) {
        if (b.number == number) {
            if (b.enabled != enabled) {
                b.enabled = enabled;
                Q_EMIT changed();
            }
            return true;
        }
    }
    return false;
}

const Breakpoint* BreakpointList::find(int number) const
{
    for (const Breakpoint& b : m_list) {
        if (b.number == number)
            return &b;
    }
    return nullptr;
}

const Breakpoint* BreakpointList::at(const QString& file, int line) const
{
    for (const Breakpoint& b : m_list) {
        if (b.type == BreakpointType::FileLine && b.line == line && samePath(b.file, file))
            return &b;
    }
    return nullptr;
}

bool BreakpointList::toggle(const QString& file, int line)
{
    if (const Breakpoint* existing = at(file, line)) {
        remove(existing->number);
        return false;
    }
    Breakpoint b;
    b.file = file;
    b.line = line;
    return add(b) != 0;
}

QList<Breakpoint> BreakpointList::inFile(const QString& file) const
{
    QList<Breakpoint> found;
    for (const Breakpoint& b : m_list) {
        if (b.type == BreakpointType::FileLine && samePath(b.file, file))
            found << b;
    }
    return found;
}

void BreakpointList::renameFile(const QString& from, const QString& to)
{
    bool any = false;
    for (Breakpoint& b : m_list) {
        if (b.type == BreakpointType::FileLine && samePath(b.file, from)) {
            b.file = normalized(to);
            any = true;
        }
    }
    if (any)
        Q_EMIT changed();
}

} // namespace OFS::Debug
