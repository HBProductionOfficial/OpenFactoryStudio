#include "core/code/Snippet.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include <algorithm>

namespace OFS {

SnippetField::Type SnippetField::typeFromString(const QString& type)
{
    if (type.compare(QLatin1String("int"), Qt::CaseInsensitive) == 0)
        return Type::Int;
    if (type.compare(QLatin1String("items"), Qt::CaseInsensitive) == 0)
        return Type::Items;
    return Type::String;
}

QString SnippetField::typeToString(Type type)
{
    switch (type) {
    case Type::Int:
        return QStringLiteral("int");
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

bool Snippet::save(const QString& filePath, QString* error) const
{
    QDir().mkpath(QFileInfo(filePath).absolutePath());
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    QXmlStreamWriter xml(&file);
    xml.setAutoFormatting(true);
    xml.setAutoFormattingIndent(2);
    xml.writeStartDocument();
    xml.writeStartElement(QStringLiteral("snippet"));
    for (const SnippetField& field : fields) {
        xml.writeEmptyElement(QStringLiteral("field"));
        xml.writeAttribute(QStringLiteral("name"), field.name);
        xml.writeAttribute(QStringLiteral("title"), field.title);
        xml.writeAttribute(QStringLiteral("type"), SnippetField::typeToString(field.type));
        if (field.type == SnippetField::Type::Items) {
            xml.writeAttribute(QStringLiteral("value"), field.values.join(QLatin1Char(',')));
            xml.writeAttribute(QStringLiteral("value_title"), field.valueTitles.join(QLatin1Char(',')));
        }
    }
    xml.writeTextElement(QStringLiteral("code"), code);
    xml.writeEndElement();
    xml.writeEndDocument();
    if (!file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
}

QString Snippet::expand(const QHash<QString, QString>& values, int* cursor) const
{
    QString text = code;
    text.replace(QLatin1String("\r\n"), QLatin1String("\n"));
    QStringList lines = text.split(QLatin1Char('\n'));
    while (!lines.isEmpty() && lines.first().trimmed().isEmpty())
        lines.removeFirst();
    while (!lines.isEmpty() && lines.last().trimmed().isEmpty())
        lines.removeLast();

    int common = -1;
    for (const QString& line : std::as_const(lines)) {
        if (line.trimmed().isEmpty())
            continue;
        const int indent = indentOf(line);
        common = common < 0 ? indent : std::min(common, indent);
    }
    for (QString& line : lines) {
        line = line.mid(std::min<qsizetype>(std::max(common, 0), indentOf(line)));
    }
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
