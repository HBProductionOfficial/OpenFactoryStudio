#pragma once

#include "core/CoreExport.h"

#include <QHash>
#include <QIcon>
#include <QString>
#include <QStringList>

namespace OFS {

// Maps action and interface names to icon files (data/iconsets/<name>/iconset.xml):
//
//   <iconset>
//       <icon name="file_open" file="files/open.svg"/>
//   </iconset>
//
// Engines ship overlays in data/engines/<engine>/iconsets/<name>/iconset.xml. In an overlay, a file
// starting with '*' refers to the global iconset ("*files/open.svg"). Lookups check the overlays
// first, then the global set.
class OFS_CORE_EXPORT IconSet
{
public:
    bool load(const QString& iconsetDir, QString* error = nullptr);
    bool addOverlay(const QString& overlayDir, QString* error = nullptr);
    void clear();

    QString name() const { return m_name; }
    bool contains(const QString& iconName) const;
    QString filePath(const QString& iconName) const;
    QIcon icon(const QString& iconName) const;
    QStringList names() const;

    static QStringList available(const QStringList& searchDirs);

private:
    bool parse(const QString& dir, bool overlay, QString* error);

    QString m_name;
    QString m_baseDir;
    QHash<QString, QString> m_files;
    mutable QHash<QString, QIcon> m_cache;
};

} // namespace OFS
