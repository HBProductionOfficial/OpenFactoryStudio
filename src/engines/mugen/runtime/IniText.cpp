#include "engines/mugen/runtime/IniText.h"

namespace OFS::Mugen::Runtime {

namespace {

QString withoutComment(const QString& line)
{
    bool quoted = false;
    for (int i = 0; i < line.size(); ++i) {
        const QChar c = line.at(i);
        if (c == QLatin1Char('"'))
            quoted = !quoted;
        else if (c == QLatin1Char(';') && !quoted)
            return line.left(i);
    }
    return line;
}

} // namespace

QString IniSection::value(const QString& key, const QString& fallback) const
{
    for (const IniEntry& entry : entries) {
        if (entry.key.compare(key, Qt::CaseInsensitive) == 0)
            return entry.value;
    }
    return fallback;
}

bool IniSection::has(const QString& key) const
{
    for (const IniEntry& entry : entries) {
        if (entry.key.compare(key, Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

IniText IniText::parse(const QString& text)
{
    IniText ini;
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (int i = 0; i < lines.size(); ++i) {
        const QString line = withoutComment(lines.at(i)).trimmed();
        if (line.isEmpty())
            continue;
        if (line.startsWith(QLatin1Char('['))) {
            const int close = int(line.indexOf(QLatin1Char(']')));
            IniSection section;
            section.name = (close > 0 ? line.mid(1, close - 1) : line.mid(1)).trimmed();
            section.line = i;
            ini.m_sections << section;
            continue;
        }
        if (ini.m_sections.isEmpty())
            continue;
        const int equals = int(line.indexOf(QLatin1Char('=')));
        IniEntry entry;
        entry.line = i;
        if (equals < 0) {
            entry.key = line;
        } else {
            entry.key = line.left(equals).trimmed();
            entry.value = line.mid(equals + 1).trimmed();
        }
        ini.m_sections.last().entries << entry;
    }
    return ini;
}

const IniSection* IniText::section(const QString& name) const
{
    for (const IniSection& section : m_sections) {
        if (section.name.compare(name, Qt::CaseInsensitive) == 0)
            return &section;
    }
    return nullptr;
}

QString unquote(const QString& value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.size() >= 2 && trimmed.startsWith(QLatin1Char('"')) && trimmed.endsWith(QLatin1Char('"')))
        return trimmed.mid(1, trimmed.size() - 2);
    return trimmed;
}

} // namespace OFS::Mugen::Runtime
