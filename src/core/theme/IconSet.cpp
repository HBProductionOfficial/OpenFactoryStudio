#include "core/theme/IconSet.h"

#include "core/settings/XmlRecordFile.h"

#include <QDir>
#include <QFileInfo>

namespace OFS {

bool IconSet::load(const QString& iconsetDir, QString* error)
{
    clear();
    m_baseDir = QDir::cleanPath(iconsetDir);
    m_name = QFileInfo(m_baseDir).fileName();
    return parse(m_baseDir, false, error);
}

bool IconSet::addOverlay(const QString& overlayDir, QString* error)
{
    return parse(QDir::cleanPath(overlayDir), true, error);
}

void IconSet::clear()
{
    m_name.clear();
    m_baseDir.clear();
    m_files.clear();
    m_cache.clear();
}

bool IconSet::parse(const QString& dir, bool overlay, QString* error)
{
    const QString path = dir + QStringLiteral("/iconset.xml");
    const auto result = XmlRecordFile::read(path, QStringLiteral("iconset"), QStringLiteral("icon"));
    if (!result.ok()) {
        if (error)
            *error = result.error;
        return false;
    }
    for (const XmlRecord& record : result.records) {
        const QString name = record.value(QStringLiteral("name"));
        QString file = record.value(QStringLiteral("file"));
        if (name.isEmpty() || file.isEmpty())
            continue;
        if (overlay && file.startsWith(QLatin1Char('*')))
            file = m_baseDir + QLatin1Char('/') + file.mid(1);
        else
            file = dir + QLatin1Char('/') + file;
        m_files.insert(name, QDir::cleanPath(file));
        m_cache.remove(name);
    }
    return true;
}

bool IconSet::contains(const QString& iconName) const
{
    return m_files.contains(iconName);
}

QString IconSet::filePath(const QString& iconName) const
{
    return m_files.value(iconName);
}

QIcon IconSet::icon(const QString& iconName) const
{
    auto cached = m_cache.constFind(iconName);
    if (cached != m_cache.cend())
        return *cached;
    QIcon icon;
    const QString file = m_files.value(iconName);
    if (!file.isEmpty() && QFileInfo::exists(file))
        icon = QIcon(file);
    m_cache.insert(iconName, icon);
    return icon;
}

QStringList IconSet::names() const
{
    QStringList list = m_files.keys();
    list.sort();
    return list;
}

QStringList IconSet::available(const QStringList& searchDirs)
{
    QStringList names;
    for (const QString& root : searchDirs) {
        const QDir dir(root + QStringLiteral("/iconsets"));
        for (const QString& sub : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
            if (QFileInfo::exists(dir.filePath(sub + QStringLiteral("/iconset.xml"))) && !names.contains(sub))
                names << sub;
        }
    }
    return names;
}

} // namespace OFS
