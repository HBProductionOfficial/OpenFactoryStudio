#pragma once

#include "core/CoreExport.h"

#include <QList>
#include <QObject>
#include <QString>

namespace OFS {

// Recently opened projects (history.xml):
//     <entry name="Kung Fu Man" file="C:/MUGEN/chars/kfm/kfm.def"/>
// The most recent entry comes first.
class OFS_CORE_EXPORT History : public QObject
{
    Q_OBJECT

public:
    struct Entry
    {
        QString name;
        QString file;
        bool operator==(const Entry&) const = default;
    };

    explicit History(QObject* parent = nullptr);

    void load(const QString& path);
    bool save(QString* error = nullptr) const;

    const QList<Entry>& entries() const { return m_entries; }

    void add(const QString& name, const QString& file);
    void remove(const QString& file);
    void clear();
    void setEntries(const QList<Entry>& entries);

    int maxEntries() const { return m_maxEntries; }
    void setMaxEntries(int max);

Q_SIGNALS:
    void changed();

private:
    void trim();

    QList<Entry> m_entries;
    QString m_path;
    int m_maxEntries = 10;
};

} // namespace OFS
