#pragma once

#include "core/CoreExport.h"

#include <QByteArray>
#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QSet>

class QFileSystemWatcher;
class QTimer;

namespace OFS {

class Project;
class ProjectFile;

// Options > General > Enable the file modification watcher: notices when open project files
// change on disk. Our own saves are not reported, and each change is reported once.
class OFS_CORE_EXPORT ProjectWatcher : public QObject
{
    Q_OBJECT

public:
    explicit ProjectWatcher(Project* project, QObject* parent = nullptr);
    ~ProjectWatcher() override;

    void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }

    void accept(ProjectFile* file);

Q_SIGNALS:
    void fileChanged(OFS::ProjectFile* file);

private:
    struct Stamp
    {
        qint64 size = -1;
        QDateTime modified;
        QByteArray hash;
        bool operator==(const Stamp& other) const = default;
    };
    static Stamp stampOf(const QString& path);
    void watch(ProjectFile* file);
    void onChanged(const QString& path);
    void check();

    Project* m_project;
    QFileSystemWatcher* m_watcher;
    QTimer* m_settle;
    bool m_enabled = true;
    QHash<QString, Stamp> m_stamps;
    QHash<QString, QPointer<ProjectFile>> m_files;
    QSet<QString> m_pending;
};

} // namespace OFS
