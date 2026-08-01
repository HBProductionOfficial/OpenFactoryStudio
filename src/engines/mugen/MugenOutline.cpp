#include "engines/mugen/MugenOutline.h"

#include "core/code/CodeLine.h"
#include "core/syntax/SyntaxDatabase.h"

#include <QCoreApplication>
#include <QRegularExpression>
#include <QStringList>

namespace OFS::Mugen {

namespace {

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

bool isBuiltInTrigger(const QString& key)
{
    static const QRegularExpression trigger(QStringLiteral("^trigger(all|\\d+)$"),
                                            QRegularExpression::CaseInsensitiveOption);
    return trigger.match(key).hasMatch();
}

// "State 210, Width" -> "210 - Width".
QString controllerLabel(const QString& header)
{
    const QString rest = header.trimmed().mid(5).trimmed();
    const qsizetype comma = rest.indexOf(QLatin1Char(','));
    if (comma < 0)
        return rest;
    return rest.left(comma).trimmed() + QStringLiteral(" - ") + rest.mid(comma + 1).trimmed();
}

QString unquoted(QString text)
{
    text = text.trimmed();
    if (text.startsWith(QLatin1Char('"')))
        text.remove(0, 1);
    if (text.endsWith(QLatin1Char('"')))
        text.chop(1);
    return text;
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
    if (t == QLatin1String("string") || t == QLatin1String("ustring"))
        return QStringLiteral("code_string");
    if (t == QLatin1String("file"))
        return QStringLiteral("code_file");
    if (t == QLatin1String("keyword"))
        return QStringLiteral("code_keyword");
    if (t == QLatin1String("keyseq"))
        return QStringLiteral("code_cmd");
    if (t == QLatin1String("hitflags") || t == QLatin1String("guardflags") || t == QLatin1String("hitattr"))
        return QStringLiteral("code_flags");
    static const QRegularExpression expression(QStringLiteral("^exp(\\d?)([ifb]?)(keyword|vec)?$"));
    if (const auto m = expression.match(t); m.hasMatch()) {
        if (m.captured(2) == QLatin1String("b"))
            return QStringLiteral("code_bool");
        if (m.captured(2) == QLatin1String("f"))
            return QStringLiteral("code_float");
        if (m.captured(2) == QLatin1String("i") || !m.captured(3).isEmpty())
            return QStringLiteral("code_int");
        return QStringLiteral("code_exp");
    }
    if (t.startsWith(QLatin1String("int")) || t.startsWith(QLatin1String("float")) ||
        t.startsWith(QLatin1String("ifloat")))
        return QStringLiteral("code_coord");
    return {};
}

Outline mugenOutline(const QString& text, const SyntaxDatabase* syntax, const QString& fileType)
{
    Outline outline;
    const QStringList lines = text.split(QLatin1Char('\n'));

    enum class Owner
    {
        None,
        Section,
        StateDef,
        Controller,
        Command
    };
    Owner owner = Owner::None;
    int commandsIndex = -1;
    int stateDefIndex = -1;
    const SyntaxBlock* currentBlock = nullptr;

    auto sectionType = [&lines](int from) {
        for (int m = from + 1; m < lines.size(); ++m) {
            const CodeLine l = CodeLine::parse(lines[m]);
            if (l.kind == CodeLine::Kind::Section)
                break;
            if (l.kind == CodeLine::Kind::KeyValue && l.key.compare(QLatin1String("type"), Qt::CaseInsensitive) == 0)
                return l.value;
        }
        return QString();
    };
    auto paramIcon = [&](const QString& key) -> QString {
        if (owner == Owner::Controller && isBuiltInTrigger(key))
            return QStringLiteral("code_bool");
        if (!syntax || fileType.isEmpty() || !currentBlock)
            return {};
        const SyntaxParam* param = syntax->param(fileType, *currentBlock, key);
        if (!param)
            return {};
        if (key.contains(QLatin1String("version"), Qt::CaseInsensitive))
            return QStringLiteral("code_version");
        return valueTypeIcon(param->type);
    };
    auto current = [&]() -> OutlineNode* {
        switch (owner) {
        case Owner::Section:
            return &outline.last();
        case Owner::StateDef:
            return &outline[stateDefIndex];
        case Owner::Controller:
            return &outline[stateDefIndex].children.last();
        case Owner::Command:
            return &outline[commandsIndex].children.last();
        case Owner::None:
            break;
        }
        return nullptr;
    };

    for (int n = 0; n < lines.size(); ++n) {
        const CodeLine line = CodeLine::parse(lines[n]);
        if (line.kind == CodeLine::Kind::Section) {
            const QString word = line.header.section(QLatin1Char(' '), 0, 0).section(QLatin1Char(','), 0, 0);
            const QString typeValue = sectionType(n);
            currentBlock =
                syntax && !fileType.isEmpty() ? syntax->findBlock(fileType, line.header, typeValue) : nullptr;
            OutlineNode node;
            node.line = n;
            if (word.compare(QLatin1String("Statedef"), Qt::CaseInsensitive) == 0) {
                const QString number = line.header.mid(word.size()).trimmed();
                const QString name = n > 0 ? commentName(lines[n - 1]) : QString();
                node.text = name.isEmpty() ? number : number + QStringLiteral(" - ") + name;
                node.icon = stateTypeIcon(typeValue);
                outline << node;
                stateDefIndex = int(outline.size()) - 1;
                owner = Owner::StateDef;
                continue;
            }
            if (word.compare(QLatin1String("State"), Qt::CaseInsensitive) == 0 && stateDefIndex >= 0) {
                node.text = controllerLabel(line.header);
                outline[stateDefIndex].children << node;
                owner = Owner::Controller;
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
                node.icon = QStringLiteral("st_command");
                outline[commandsIndex].children << node;
                owner = Owner::Command;
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
            owner = Owner::Section;
            stateDefIndex = -1;
            continue;
        }
        if (line.kind != CodeLine::Kind::KeyValue || owner == Owner::None)
            continue;
        OutlineNode* node = current();
        if (owner == Owner::Command && line.key.compare(QLatin1String("name"), Qt::CaseInsensitive) == 0)
            node->text = unquoted(line.value);
        if (owner == Owner::StateDef)
            continue;
        OutlineNode child;
        child.text = line.key.toLower();
        child.icon = paramIcon(line.key);
        child.line = n;
        node->children << child;
    }
    return outline;
}

} // namespace OFS::Mugen
