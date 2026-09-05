#pragma once

#include <QList>
#include <QString>
#include <QStringList>

namespace OFS::Mugen::Runtime {

// A M.U.G.E.N definition file as the engine reads it. Section names and keys may repeat ([State],
// trigger1). Comments and blank lines are dropped and values are trimmed, keeping quotes.
struct IniEntry
{
    QString key;
    QString value;
    int line = 0;
};

struct IniSection
{
    QString name;
    int line = 0;
    QList<IniEntry> entries;

    QString value(const QString& key, const QString& fallback = QString()) const;
    bool has(const QString& key) const;
};

class IniText
{
public:
    static IniText parse(const QString& text);

    const QList<IniSection>& sections() const { return m_sections; }
    const IniSection* section(const QString& name) const;

private:
    QList<IniSection> m_sections;
};

QString unquote(const QString& value);

} // namespace OFS::Mugen::Runtime
