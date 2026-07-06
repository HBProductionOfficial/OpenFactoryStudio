#pragma once

#include "core/CoreExport.h"

#include <QColor>
#include <QMap>
#include <QString>
#include <QStringList>

namespace OFS {

// Colors of the code editors and graphic workspaces (data/colorsets/<name>.xml):
//
//   <colors>
//       <color name="keyword" value="255,255,255" bold="true"/>
//       <color name="comment" value="128,128,128" italic="true"/>
//   </colors>
//
// Known roles: background, foreground, line, block_range, paren, paren_range, comment, block,
// block_type, number, string, keyword, keyword_value, function, const, symbol, back_start,
// back_end, attackbox, collisionbox, sound_wave.
class OFS_CORE_EXPORT ColorSet
{
public:
    struct Entry
    {
        QColor color;
        bool bold = false;
        bool italic = false;
        bool operator==(const Entry&) const = default;
    };

    bool load(const QString& path, QString* error = nullptr);
    bool save(const QString& path, QString* error = nullptr) const;

    bool contains(const QString& role) const { return m_entries.contains(role); }
    Entry entry(const QString& role) const { return m_entries.value(role); }
    QColor color(const QString& role, const QColor& fallback = {}) const;
    void set(const QString& role, const Entry& entry) { m_entries.insert(role, entry); }
    QStringList roles() const { return m_entries.keys(); }

    QString name() const { return m_name; }
    void setName(const QString& name) { m_name = name; }

    bool operator==(const ColorSet& other) const { return m_entries == other.m_entries; }

private:
    QString m_name;
    QMap<QString, Entry> m_entries;
};

} // namespace OFS
