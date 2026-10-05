#include "core/project/ProjectWatcher.h"

#include "core/project/Project.h"
#include "core/project/ProjectFile.h"

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QTimer>

namespace OFS {

ProjectWatcher::ProjectWatcher(Project* project, QObject* parent)
    : QObject(parent)
    , m_project(project)
    , m_watcher(new QFileSystemWatcher(this))
    , m_settle(new QTimer(this))
{
    // Writers take a moment, so changes are checked once the file has settled.
    m_settle->setSingleShot(true);
    m_settle->setInterval(200);
    connect(m_settle, &QTimer::timeout, this, &ProjectWatcher::check);
    connect(m_watcher, &QFileSystemWatcher::fileChanged, this, &ProjectWatcher::onChanged);
    for (ProjectFile* file : project->files())
        watch(file);
    connect(project, &Project::fileAdded, this, &ProjectWatcher::watch);
}

ProjectWatcher::~ProjectWatcher() = default;

ProjectWatcher::Stamp ProjectWatcher::stampOf(const QString& path)
{
    Stamp stamp;
    const QFileInfo info(path);
    if (!info.exists())
        return stamp;
    stamp.size = info.size();
    stamp.modified = info.lastModified();
    QFile file(path);
    if (file.open(QIODevice::ReadOnly)) {
        QCryptographicHash hash(QCryptographicHash::Md5);
        hash.addData(&file);
        stamp.hash = hash.result();
    }
    return stamp;
}

void ProjectWatcher::watch(ProjectFile* file)
{
    if (!file)
        return;
    connect(file, &ProjectFile::saved, this, [this, file] { accept(file); });
    connect(file, &ProjectFile::pathChanged, this, [this, file](const QString& oldPath) {
        m_watcher->removePath(oldPath);
        m_stamps.remove(oldPath);
        m_files.remove(oldPath);
        accept(file);
    });
    accept(file);
}

void ProjectWatcher::accept(ProjectFile* file)
{
    if (!file)
        return;
    const QString path = file->path();
    m_pending.remove(path);
    if (file->isUntitled() || !QFileInfo::exists(path)) {
        m_watcher->removePath(path);
        m_stamps.remove(path);
        m_files.remove(path);
        return;
    }
    m_files.insert(path, file);
    m_stamps.insert(path, stampOf(path));
    // Safe saves replace the file by renaming, which drops the watch.
    if (!m_watcher->files().contains(path))
        m_watcher->addPath(path);
}

void ProjectWatcher::setEnabled(bool enabled)
{
    if (enabled == m_enabled)
        return;
    m_enabled = enabled;
    m_pending.clear();
    if (enabled) {
        for (const QPointer<ProjectFile>& file : std::as_const(m_files))
            accept(file);
    }
}

void ProjectWatcher::onChanged(const QString& path)
{
    if (!m_enabled || !m_files.contains(path))
        return;
    m_pending.insert(path);
    m_settle->start();
}

void ProjectWatcher::check()
{
    const QSet<QString> pending = m_pending;
    m_pending.clear();
    for (const QString& path : pending) {
        ProjectFile* file = m_files.value(path);
        if (!file)
            continue;
        // Still saving it ourselves. Look again after the save, when its stamp is taken, so our
        // own writes never count as a change.
        if (file->isSaving()) {
            m_pending.insert(path);
            m_settle->start();
            continue;
        }
        if (!m_watcher->files().contains(path) && QFileInfo::exists(path))
            m_watcher->addPath(path);
        const Stamp now = stampOf(path);
        if (now.size < 0 || now == m_stamps.value(path))
            continue;
        // Report each change once, whether the user declines it or not.
        m_stamps.insert(path, now);
        Q_EMIT fileChanged(file);
    }
}

} // namespace OFS
