#include "engines/mugen/MugenOutline.h"

#include "core/code/CodeLine.h"
#include "core/syntax/SyntaxDatabase.h"

#include <QCoreApplication>
#include <QStringList>

namespace OFS::Mugen {

namespace {

std::pair<QString, QString> splitHeader(const QString& header)
{
    const QString h = header.trimmed();
    const qsizetype space = h.indexOf(QLatin1Char(' '));
    if (space < 0)
        return {h, {}};
    return {h.left(space), h.mid(space + 1).trimmed()};
}

QString commentName(const QString& line)
{
    const CodeLine parsed = CodeLine::parse(line);
    if (parsed.kind != CodeLine::Kind::Comment)
        return {};
    QString name = line.trimmed();
    while (name.startsWith(QLatin1Char(';')))
        name.remove(0, 1);
    name = name.trimmed();
    for (QChar c : name) {
        if (c.isLetterOrNumber())
            return name;
    }
    return {};
}

QString stateTypeIcon(const QString& type)
{
    const QString t = type.trimmed().toLower();
    if (t.isEmpty())
        return {};
    switch (t.at(0).unicode()) {
    case 's':
        return QStringLiteral("st_s");
    case 'c':
        return QStringLiteral("st_c");
    case 'a':
        return QStringLiteral("st_a");
    case 'l':
        return QStringLiteral("st_l");
    case 'u':
        return QStringLiteral("st_u");
    default:
        return {};
    }
}

} // namespace

QString valueTypeIcon(const QString& type)
{
    const QString t = type.toLower();
    if (t.isEmpty())
        return {};
    if (t == QLatin1String("int") || t == QLatin1String("intvec"))
        return QStringLiteral("code_int");
    if (t == QLatin1String("float"))
        return QStringLiteral("code_float");
    if (t == QLatin1String("string"))
        return QStringLiteral("code_string");
    if (t == QLatin1String("ustring"))
        return QStringLiteral("code_ustring");
    if (t == QLatin1String("file"))
        return QStringLiteral("code_file");
    if (t == QLatin1String("keyword") || t.endsWith(QLatin1String("keyword")))
        return QStringLiteral("code_keyword");
    if (t == QLatin1String("keyseq"))
        return QStringLiteral("code_cmd");
    if (t == QLatin1String("hitflags") || t == QLatin1String("guardflags") || t == QLatin1String("hitattr"))
        return QStringLiteral("code_flags");
    if (t.startsWith(QLatin1String("exp")))
        return QStringLiteral("code_exp");
    if (t.startsWith(QLatin1String("int")) || t.startsWith(QLatin1String("float")) ||
        t.startsWith(QLatin1String("ifloat")))
        return QStringLiteral("code_coord");
    return {};
}

Outline mugenOutline(const QString& text, const SyntaxDatabase* syntax, const QString& fileType)
{
    Outline outline;
    const QStringList lines = text.split(QLatin1Char('\n'));

    int commandsIndex = -1;
    OutlineNode* current = nullptr;
    int stateDefIndex = -1;
    QString currentHeader;
    QString typeValue;

    auto paramIcon = [&](const QString& key) -> QString {
        if (!syntax || fileType.isEmpty())
            return {};
        const SyntaxBlock* block = syntax->findBlock(fileType, currentHeader, typeValue);
        if (!block)
            return {};
        const SyntaxParam* param = syntax->param(fileType, *block, key);
        return param ? valueTypeIcon(param->type) : QString();
    };

    for (int n = 0; n < lines.size(); ++n) {
        const CodeLine line = CodeLine::parse(lines[n]);
        if (line.kind == CodeLine::Kind::Section) {
            const auto [word, rest] = splitHeader(line.header);
            currentHeader = line.header;
            typeValue.clear();
            OutlineNode node;
            node.line = n;
            if (word.compare(QLatin1String("Statedef"), Qt::CaseInsensitive) == 0) {
                const QString name = n > 0 ? commentName(lines[n - 1]) : QString();
                node.text = name.isEmpty() ? rest : rest + QStringLiteral(" - ") + name;
                outline << node;
                stateDefIndex = int(outline.size()) - 1;
                current = &outline.last();
                continue;
            }
            if (word.compare(QLatin1String("State"), Qt::CaseInsensitive) == 0 && stateDefIndex >= 0) {
                node.text = line.header;
                outline[stateDefIndex].children << node;
                current = &outline[stateDefIndex].children.last();
                continue;
            }
            if (line.header.compare(QLatin1String("Command"), Qt::CaseInsensitive) == 0) {
                if (commandsIndex < 0) {
                    OutlineNode group;
                    group.text = QCoreApplication::translate("MugenOutline", "Commands");
                    group.line = n;
                    outline << group;
                    commandsIndex = int(outline.size()) - 1;
                }
                node.text = line.header;
                node.icon = QStringLiteral("st_command");
                outline[commandsIndex].children << node;
                current = &outline[commandsIndex].children.last();
                stateDefIndex = -1;
                continue;
            }
            // "ja.Quotes" -> "Quotes [ja]"
            QString caption = line.header;
            if (caption.size() > 3 && caption.at(2) == QLatin1Char('.') && caption.at(0).isLetter() &&
                caption.at(1).isLetter())
                caption = caption.mid(3) + QStringLiteral(" [") + caption.left(2) + QLatin1Char(']');
            node.text = caption;
            if (caption.startsWith(QLatin1String("Quotes"), Qt::CaseInsensitive))
                node.icon = QStringLiteral("st_quote");
            outline << node;
            current = &outline.last();
            stateDefIndex = -1;
            continue;
        }
        if (line.kind != CodeLine::Kind::KeyValue || !current)
            continue;

        const bool isType = line.key.compare(QLatin1String("type"), Qt::CaseInsensitive) == 0;
        if (isType)
            typeValue = line.value;
        if (isType && stateDefIndex >= 0 && current == &outline[stateDefIndex])
            current->icon = stateTypeIcon(line.value);
        if (commandsIndex >= 0 && !outline[commandsIndex].children.isEmpty() &&
            current == &outline[commandsIndex].children.last()) {
            if (line.key.compare(QLatin1String("name"), Qt::CaseInsensitive) == 0) {
                QString name = line.value;
                if (name.startsWith(QLatin1Char('"')))
                    name = name.mid(1);
                if (name.endsWith(QLatin1Char('"')))
                    name.chop(1);
                current->text = name;
            } else if (line.key.compare(QLatin1String("command"), Qt::CaseInsensitive) == 0) {
                current->text += QStringLiteral(": ") + line.value;
            }
            continue;
        }
        OutlineNode child;
        child.text = line.key + (line.index.isEmpty() ? QString() : QLatin1Char('(') + line.index + QLatin1Char(')')) +
                     QStringLiteral(" = ") + line.value;
        child.icon = paramIcon(line.key);
        child.line = n;
        current->children << child;
    }
    return outline;
}

} // namespace OFS::Mugen
