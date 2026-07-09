#include "engines/mugen/DefFile.h"

#include <QFile>
#include <QRegularExpression>
#include <QStringConverter>

namespace OFS::Mugen {

namespace {

QString stripComment(const QString& line)
{
    bool quoted = false;
    for (qsizetype i = 0; i < line.size(); ++i) {
        const QChar c = line.at(i);
        if (c == QLatin1Char('"'))
            quoted = !quoted;
        else if (c == QLatin1Char(';') && !quoted)
            return line.left(i);
    }
    return line;
}

} // namespace

bool DefFile::load(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const QByteArray data = file.readAll();
    // Definition files are usually ASCII or UTF-8; fall back to Latin-1 for other 8-bit encodings.
    auto decoder = QStringDecoder(QStringDecoder::Utf8);
    QString text = decoder(data);
    if (decoder.hasError())
        text = QString::fromLatin1(data);
    parse(text);
    return true;
}

void DefFile::parse(const QString& text)
{
    m_sections.clear();
    Section* current = nullptr;
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (QString line : lines) {
        line = stripComment(line).trimmed();
        if (line.isEmpty())
            continue;
        if (line.startsWith(QLatin1Char('['))) {
            const qsizetype end = line.indexOf(QLatin1Char(']'));
            Section section;
            section.name = (end > 0 ? line.mid(1, end - 1) : line.mid(1)).trimmed();
            m_sections.append(section);
            current = &m_sections.last();
            continue;
        }
        if (!current)
            continue;
        const qsizetype eq = line.indexOf(QLatin1Char('='));
        if (eq < 0)
            continue;
        current->values.append({line.left(eq).trimmed(), unquote(line.mid(eq + 1).trimmed())});
    }
}

const DefFile::Section* DefFile::find(const QString& name) const
{
    for (const Section& s : m_sections) {
        if (s.name.compare(name, Qt::CaseInsensitive) == 0)
            return &s;
    }
    return nullptr;
}

bool DefFile::hasSection(const QString& name) const
{
    return find(name) != nullptr;
}

bool DefFile::hasSectionStartingWith(const QString& prefix) const
{
    for (const Section& s : m_sections) {
        if (s.name.startsWith(prefix, Qt::CaseInsensitive))
            return true;
    }
    return false;
}

QString DefFile::value(const QString& section, const QString& key, const QString& fallback) const
{
    if (const Section* s = find(section)) {
        for (const auto& kv : s->values) {
            if (kv.first.compare(key, Qt::CaseInsensitive) == 0)
                return kv.second;
        }
    }
    return fallback;
}

QStringList DefFile::values(const QString& section, const QString& keyPattern) const
{
    QStringList result;
    const QRegularExpression re(QStringLiteral("^(?:") + keyPattern + QStringLiteral(")$"),
                                QRegularExpression::CaseInsensitiveOption);
    if (const Section* s = find(section)) {
        for (const auto& kv : s->values) {
            if (re.match(kv.first).hasMatch())
                result << kv.second;
        }
    }
    return result;
}

QString DefFile::unquote(const QString& value)
{
    if (value.size() >= 2 && value.startsWith(QLatin1Char('"')) && value.endsWith(QLatin1Char('"')))
        return value.mid(1, value.size() - 2);
    if (value.startsWith(QLatin1Char('"')))
        return value.mid(1);
    return value;
}

} // namespace OFS::Mugen
