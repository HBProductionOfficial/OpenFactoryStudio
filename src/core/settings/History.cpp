#include "core/settings/History.h"

#include "core/settings/XmlRecordFile.h"

#include <QDir>
#include <QFileInfo>

namespace OFS {

namespace {

const QString kRoot = QStringLiteral("history");
const QString kElement = QStringLiteral("entry");
const QString kName = QStringLiteral("name");
const QString kFile = QStringLiteral("file");

bool samePath(const QString& a, const QString& b)
{
#if defined(Q_OS_WIN)
    return QDir::cleanPath(a).compare(QDir::cleanPath(b), Qt::CaseInsensitive) == 0;
#else
    return QDir::cleanPath(a) == QDir::cleanPath(b);
#endif
}

} // namespace

History::History(QObject* parent)
    : QObject(parent)
{}

void History::load(const QString& path)
{
    m_path = path;
    m_entries.clear();
    if (QFileInfo::exists(path)) {
        const auto result = XmlRecordFile::read(path, kRoot, kElement);
        for (const XmlRecord& record : result.records) {
            const QString file = record.value(kFile);
            if (!file.isEmpty())
                m_entries.append({record.value(kName), file});
        }
    }
    Q_EMIT changed();
}

bool History::save(QString* error) const
{
    if (m_path.isEmpty())
        return false; // never loaded: nowhere to write
    QList<XmlRecord> records;
    for (const Entry& entry : m_entries)
        records.append(XmlRecord{{kName, entry.name}, {kFile, entry.file}});
    return XmlRecordFile::write(m_path, kRoot, kElement, records, error);
}

void History::add(const QString& name, const QString& file)
{
    const QString normalized = QDir::fromNativeSeparators(file);
    for (qsizetype i = 0; i < m_entries.size(); ++i) {
        if (samePath(m_entries[i].file, normalized)) {
            m_entries.removeAt(i);
            break;
        }
    }
    m_entries.prepend({name, normalized});
    trim();
    Q_EMIT changed();
}

void History::remove(const QString& file)
{
    for (qsizetype i = 0; i < m_entries.size(); ++i) {
        if (samePath(m_entries[i].file, file)) {
            m_entries.removeAt(i);
            Q_EMIT changed();
            return;
        }
    }
}

void History::clear()
{
    m_entries.clear();
    Q_EMIT changed();
}

void History::setEntries(const QList<Entry>& entries)
{
    m_entries = entries;
    trim();
    Q_EMIT changed();
}

void History::setMaxEntries(int max)
{
    m_maxEntries = qMax(0, max);
    trim();
}

void History::trim()
{
    while (m_entries.size() > m_maxEntries)
        m_entries.removeLast();
}

} // namespace OFS
