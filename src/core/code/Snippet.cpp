#include "core/code/Snippet.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QXmlStreamReader>

#include <algorithm>

namespace OFS {

SnippetField::Type SnippetField::typeFromString(const QString& type)
{
    if (type.compare(QLatin1String("int"), Qt::CaseInsensitive) == 0)
        return Type::Int;
    if (type.compare(QLatin1String("float"), Qt::CaseInsensitive) == 0)
        return Type::Float;
    if (type.compare(QLatin1String("items"), Qt::CaseInsensitive) == 0)
        return Type::Items;
    return Type::String;
}

QString SnippetField::typeToString(Type type)
{
    switch (type) {
    case Type::Int:
        return QStringLiteral("int");
    case Type::Float:
        return QStringLiteral("float");
    case Type::Items:
        return QStringLiteral("items");
    case Type::String:
        break;
    }
    return QStringLiteral("string");
}

namespace {

QStringList splitList(const QString& text)
{
    if (text.isEmpty())
        return {};
    QStringList items = text.split(QLatin1Char(','));
    for (QString& item : items)
        item = item.trimmed();
    return items;
}

int indentOf(const QString& line)
{
    int n = 0;
    while (n < line.size() && (line.at(n) == QLatin1Char(' ') || line.at(n) == QLatin1Char('\t')))
        ++n;
    return n;
}

} // namespace

bool Snippet::load(const QString& filePath, QString* error)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    QXmlStreamReader xml(&file);
    if (!xml.readNextStartElement() || xml.name() != QLatin1String("snippet")) {
        if (error)
            *error = xml.hasError() ? xml.errorString() : QStringLiteral("not a snippet file");
        return false;
    }
    QList<SnippetField> readFields;
    QString readCode;
    while (xml.readNextStartElement()) {
        if (xml.name() == QLatin1String("field")) {
            const QXmlStreamAttributes a = xml.attributes();
            SnippetField field;
            field.name = a.value(QLatin1String("name")).toString();
            field.title = a.value(QLatin1String("title")).toString();
            field.type = SnippetField::typeFromString(a.value(QLatin1String("type")).toString());
            field.values = splitList(a.value(QLatin1String("value")).toString());
            field.valueTitles = splitList(a.value(QLatin1String("value_title")).toString());
            if (!field.name.isEmpty())
                readFields << field;
            xml.skipCurrentElement();
        } else if (xml.name() == QLatin1String("code")) {
            readCode = xml.readElementText();
        } else {
            xml.skipCurrentElement();
        }
    }
    if (xml.hasError()) {
        if (error)
            *error = QStringLiteral("%1:%2: %3").arg(xml.lineNumber()).arg(xml.columnNumber()).arg(xml.errorString());
        return false;
    }
    name = QFileInfo(filePath).completeBaseName();
    path = filePath;
    fields = readFields;
    code = readCode;
    return true;
}

namespace {

QString xmlEscaped(const QString& text, bool attribute)
{
    QString out = text.toHtmlEscaped();
    if (!attribute)
        out.replace(QLatin1String("&quot;"), QLatin1String("\""));
    return out;
}

bool writeFile(const QString& filePath, const QByteArray& bytes, QString* error)
{
    QDir().mkpath(QFileInfo(filePath).absolutePath());
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
}

} // namespace

bool Snippet::save(const QString& filePath, QString* error) const
{
    QStringList lines;
    lines << QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?>") << QStringLiteral("<snippet>") << QString();
    for (const SnippetField& field : fields) {
        QString line = QStringLiteral("\t<field name=\"%1\" title=\"%2\" type=\"%3\"")
                           .arg(xmlEscaped(field.name, true), xmlEscaped(field.title, true),
                                SnippetField::typeToString(field.type));
        if (field.type == SnippetField::Type::Items)
            line += QStringLiteral(" value=\"%1\" value_title=\"%2\"")
                        .arg(xmlEscaped(field.values.join(QLatin1Char(',')), true),
                             xmlEscaped(field.valueTitles.join(QLatin1Char(',')), true));
        lines << line + QStringLiteral(" />");
    }
    if (!fields.isEmpty())
        lines << QString();
    lines << QStringLiteral("\t<code>");
    QString text = code;
    text.replace(QLatin1String("\r\n"), QLatin1String("\n"));
    QStringList codeLines = text.split(QLatin1Char('\n'));
    if (codeLines.size() > 1 && codeLines.first().trimmed().isEmpty())
        codeLines.removeFirst(); // the line break after <code> read from a file
    while (codeLines.size() > 1 && codeLines.last().trimmed().isEmpty())
        codeLines.removeLast(); // the line before </code> gives the final newline back on loading
    for (const QString& line : std::as_const(codeLines))
        lines << QStringLiteral("\t\t") + xmlEscaped(line, false);
    lines << QStringLiteral("\t</code>") << QString() << QStringLiteral("</snippet>");
    return writeFile(filePath, (lines.join(QStringLiteral("\r\n")) + QStringLiteral("\r\n")).toUtf8(), error);
}

bool Snippet::saveTranslationSource(const QString& filePath, QString* error) const
{
    const QString source = xmlEscaped(name, false);
    const QString ts = QStringLiteral("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
                                      "<!DOCTYPE TS>\n"
                                      "<TS version=\"2.0\">\n"
                                      "<context>\n"
                                      "\t<name>OFS::SnippetLibrary</name>\n"
                                      "\t<message>\n"
                                      "\t\t<source>%1</source>\n"
                                      "\t\t<comment>%1</comment>\n"
                                      "\t\t<translation type=\"unfinished\"></translation>\n"
                                      "\t</message>\n"
                                      "</context>\n"
                                      "</TS>\n")
                           .arg(source);
    return writeFile(filePath, ts.toUtf8(), error);
}

QString Snippet::expand(const QHash<QString, QString>& values, int* cursor) const
{
    QString text = code;
    text.replace(QLatin1String("\r\n"), QLatin1String("\n"));
    QStringList lines = text.split(QLatin1Char('\n'));
    if (lines.size() > 1 && lines.first().trimmed().isEmpty())
        lines.removeFirst();
    for (QString& line : lines)
        line = line.mid(indentOf(line));
    text = lines.join(QLatin1Char('\n'));

    // Longer names first, so "@sdefname" is not taken for "@sdef" followed by "name".
    QList<QString> names = values.keys();
    std::sort(names.begin(), names.end(), [](const QString& a, const QString& b) { return a.size() > b.size(); });
    for (const QString& fieldName : std::as_const(names))
        text.replace(QLatin1Char('@') + fieldName, values.value(fieldName));

    const qsizetype marker = text.indexOf(QLatin1String("CURSOR"));
    if (marker >= 0)
        text.remove(marker, 6);
    if (cursor)
        *cursor = int(marker);
    return text;
}

void SnippetLibrary::load(const QStringList& dirs)
{
    m_snippets.clear();
    for (const QString& dir : dirs) {
        const QFileInfoList files = QDir(dir).entryInfoList({QStringLiteral("*.xml")}, QDir::Files);
        for (const QFileInfo& info : files) {
            Snippet snippet;
            if (snippet.load(info.absoluteFilePath()))
                insert(snippet);
        }
    }
}

QStringList SnippetLibrary::names() const
{
    QList<const Snippet*> list;
    for (const Snippet& s : m_snippets)
        list << &s;
    // Sorted by file name, so "Command defaults.xml" comes before "Command.xml".
    std::sort(list.begin(), list.end(), [](const Snippet* a, const Snippet* b) {
        return QString::compare(a->name + QStringLiteral(".xml"), b->name + QStringLiteral(".xml"),
                                Qt::CaseInsensitive) < 0;
    });
    QStringList result;
    for (const Snippet* s : std::as_const(list))
        result << s->name;
    return result;
}

const Snippet* SnippetLibrary::find(const QString& name) const
{
    auto it = m_snippets.constFind(name.toLower());
    return it == m_snippets.cend() ? nullptr : &it.value();
}

void SnippetLibrary::insert(const Snippet& snippet)
{
    m_snippets.insert(snippet.name.toLower(), snippet);
}

} // namespace OFS
