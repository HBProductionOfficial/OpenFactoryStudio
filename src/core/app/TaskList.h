#pragma once

#include "core/CoreExport.h"

#include <QFuture>
#include <QList>
#include <QObject>
#include <QString>

#include <functional>

namespace OFS {

// Background tasks such as loading and saving big files, shown in the status bar.
class OFS_CORE_EXPORT TaskList : public QObject
{
    Q_OBJECT

public:
    struct Task
    {
        int id = 0;
        QString name;
        int progress = -1; // percent, -1 while unknown
    };

    explicit TaskList(QObject* parent = nullptr);

    int begin(const QString& name);
    void setProgress(int id, int percent);
    void end(int id);
    const QList<Task>& tasks() const { return m_tasks; }
    bool isEmpty() const { return m_tasks.isEmpty(); }

    // Runs `work` in another thread and waits. Slow work shows as a task while the window repaints.
    void run(const QString& name, const std::function<void()>& work);

Q_SIGNALS:
    void changed();

private:
    QList<Task> m_tasks;
    int m_next = 1;
};

} // namespace OFS
