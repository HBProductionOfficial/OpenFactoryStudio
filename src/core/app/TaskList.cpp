#include "core/app/TaskList.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>
#include <QtConcurrent/QtConcurrentRun>

namespace OFS {

TaskList::TaskList(QObject* parent)
    : QObject(parent)
{}

int TaskList::begin(const QString& name)
{
    const int id = m_next++;
    m_tasks << Task{id, name, -1};
    Q_EMIT changed();
    return id;
}

void TaskList::setProgress(int id, int percent)
{
    for (Task& task : m_tasks) {
        if (task.id == id && task.progress != percent) {
            task.progress = percent;
            Q_EMIT changed();
        }
    }
}

void TaskList::end(int id)
{
    if (m_tasks.removeIf([id](const Task& task) { return task.id == id; }) > 0)
        Q_EMIT changed();
}

void TaskList::run(const QString& name, const std::function<void()>& work)
{
    QFuture<void> future = QtConcurrent::run(work);
    // Quick work just finishes. Longer work shows as a task while the window repaints.
    QElapsedTimer waited;
    waited.start();
    while (!future.isFinished() && waited.elapsed() < 150)
        QThread::msleep(2);
    if (future.isFinished())
        return;
    const int id = begin(name);
    while (!future.isFinished()) {
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 20);
        QThread::msleep(5);
    }
    end(id);
}

} // namespace OFS
