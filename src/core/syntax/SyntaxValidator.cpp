#include "core/syntax/SyntaxValidator.h"

#include "core/code/CodeLine.h"
#include "core/syntax/SyntaxDatabase.h"

#include <QCoreApplication>
#include <QHash>
#include <QRegularExpression>
#include <QSet>

namespace OFS {

namespace {

// Translation context for this file's texts. lupdate reads the qualified class name.
struct SyntaxValidatorTexts
{
    Q_DECLARE_TR_FUNCTIONS(OFS::SyntaxValidatorTexts)
};

Issue makeIssue(Issue::Severity severity, int line, int column, int length, const QString& message)
{
    Issue issue;
    issue.severity = severity;
    issue.line = line;
    issue.column = column;
    issue.length = length;
    issue.message = message;
    return issue;
}

bool isInteger(const QString& text)
{
    static const QRegularExpression re(QStringLiteral("^[+-]?\\d+$"));
    return re.match(text).hasMatch();
}

bool isNumber(const QString& text)
{
    static const QRegularExpression re(QStringLiteral("^[+-]?(\\d+\\.?\\d*|\\.\\d+)$"));
    return re.match(text).hasMatch();
}

int componentCount(const QString& type)
{
    static const QRegularExpression re(QStringLiteral("^(?:int|float|ifloat|exp|exp[ifb])(\\d)$"));
    const auto m = re.match(type);
    return m.hasMatch() ? m.captured(1).toInt() : 0;
}

struct ParsedLine
{
    int number;
    CodeLine code;
};

} // namespace

ExpressionChecker::~ExpressionChecker() = default;

void ExpressionChecker::check(const QString& text, int offset, const Context& context, IssueList& issues) const
{
    int depth = 0;
    bool quoted = false;
    for (int i = 0; i < text.size(); ++i) {
        const QChar c = text.at(i);
        if (c == QLatin1Char('"'))
            quoted = !quoted;
        else if (!quoted && c == QLatin1Char('('))
            ++depth;
        else if (!quoted && c == QLatin1Char(')') && --depth < 0) {
            issues << makeIssue(Issue::Severity::Error, context.line, offset + i, 1,
                                SyntaxValidatorTexts::tr("Unbalanced parentheses"));
            return;
        }
    }
    if (depth != 0 || quoted)
        issues << makeIssue(Issue::Severity::Error, context.line, offset, int(text.size()),
                            quoted ? SyntaxValidatorTexts::tr("Unterminated string")
                                   : SyntaxValidatorTexts::tr("Unbalanced parentheses"));
}

SyntaxValidator::SyntaxValidator(const SyntaxDatabase& syntax, const QString& fileType, const QString& version)
    : m_syntax(syntax)
    , m_fileType(fileType)
    , m_version(version)
{}

int SyntaxValidator::errorCount(const IssueList& issues)
{
    int n = 0;
    for (const Issue& i : issues) {
        if (i.isError())
            ++n;
    }
    return n;
}

bool SyntaxValidator::isBuiltIn(const QString& header) const
{
    for (const QString& s : m_builtIn) {
        if (SyntaxDatabase::headerMatchesSection(header.trimmed(), s))
            return true;
    }
    return false;
}

void SyntaxValidator::checkValue(const SyntaxParam& param, const QString& value, int offset, int line,
                                 const QString& section, const QString& blockType, IssueList& issues) const
{
    const QString type = param.type.toLower();
    const QString v = value.trimmed();
    if (v.isEmpty()) {
        if (param.required && !param.nil)
            issues << makeIssue(Issue::Severity::Error, line, offset, 1,
                                SyntaxValidatorTexts::tr("Missing value for parameter '%1'").arg(param.name));
        return;
    }

    auto badValue = [&](const QString& what) {
        issues << makeIssue(
            Issue::Severity::Error, line, offset, int(value.size()),
            SyntaxValidatorTexts::tr("Invalid value '%1' for parameter '%2' (%3 expected)").arg(v, param.name, what));
    };

    if (type.startsWith(QLatin1String("exp")) || type == QLatin1String("hitattr") ||
        type == QLatin1String("hitflags") || type == QLatin1String("guardflags") || type == QLatin1String("keyseq")) {
        if (m_checker) {
            // "F5,2" / "S10": a prefix letter picks the fight data's (F) or the player's own (S) sounds,
            // animations and sparks. Only the number is checked.
            QString checked = value;
            if (!param.prefixes.isEmpty()) {
                static const QRegularExpression prefixed(QStringLiteral("(?:^|,)\\s*([A-Za-z])(?=\\s*\\d)"));
                for (auto it = prefixed.globalMatch(value); it.hasNext();) {
                    const QRegularExpressionMatch m = it.next();
                    if (param.prefixes.contains(m.captured(1), Qt::CaseInsensitive))
                        checked[m.capturedStart(1)] = QLatin1Char(' ');
                }
            }
            ExpressionChecker::Context ctx;
            ctx.fileType = m_fileType;
            ctx.section = section;
            ctx.blockType = blockType;
            ctx.paramType = param.type;
            ctx.version = m_version;
            ctx.list = param.list;
            ctx.line = line;
            m_checker->check(checked, offset, ctx, issues);
        }
        return;
    }

    if (type == QLatin1String("int")) {
        QString n = v;
        if (!param.prefixes.isEmpty() && !n.isEmpty() && param.prefixes.contains(n.at(0), Qt::CaseInsensitive))
            n = n.mid(1);
        if (!isInteger(n) && !(param.nil && n == QLatin1String("-1")))
            badValue(SyntaxValidatorTexts::tr("integer"));
        return;
    }
    if (type == QLatin1String("float")) {
        if (!isNumber(v))
            badValue(SyntaxValidatorTexts::tr("number"));
        return;
    }
    const int count = componentCount(type);
    if (count > 0 || type == QLatin1String("intvec")) {
        const QStringList parts = v.split(QLatin1Char(','));
        const int max = count > 0 ? count : (param.max > 0 ? param.max : 1024);
        if (parts.size() > max) {
            issues << makeIssue(
                Issue::Severity::Warning, line, offset, int(value.size()),
                SyntaxValidatorTexts::tr("Too many values for parameter '%1' (%2 at most)").arg(param.name).arg(max));
        }
        const bool integers = type.startsWith(QLatin1String("int")) || type == QLatin1String("intvec");
        for (qsizetype i = 0; i < parts.size() && i < max; ++i) {
            const QString p = parts[i].trimmed();
            const bool required = i < param.optional.size() && param.optional[i] == QLatin1String("req");
            if (p.isEmpty()) {
                if (required)
                    badValue(SyntaxValidatorTexts::tr("%1 values").arg(max));
                continue;
            }
            // ifloat3: the first component is an integer, the others numbers.
            const bool wantInt = integers || (type == QLatin1String("ifloat3") && i == 0);
            if (wantInt ? !isInteger(p) : !isNumber(p)) {
                badValue(wantInt ? SyntaxValidatorTexts::tr("integers") : SyntaxValidatorTexts::tr("numbers"));
                return;
            }
        }
        return;
    }
    if (type == QLatin1String("keyword")) {
        if (param.list.isEmpty())
            return;
        for (const QString& part : v.split(QLatin1Char(','))) {
            const QString word = part.trimmed();
            if (word.isEmpty())
                continue;
            if (!SyntaxDatabase::keywordMatches(param.list, word)) {
                const int pos = int(value.indexOf(word));
                issues << makeIssue(Issue::Severity::Error, line, offset + qMax(0, pos), int(word.size()),
                                    SyntaxValidatorTexts::tr("Unknown keyword '%1'").arg(word));
            }
        }
    }
}

IssueList SyntaxValidator::validate(const QString& text) const
{
    IssueList issues;
    const SyntaxFileType* fileType = m_syntax.fileType(m_fileType);
    if (!fileType || fileType->blocks.isEmpty())
        return issues;

    const QStringList rawLines = text.split(QLatin1Char('\n'));

    // Group the lines by section. Typed sections are only known once their "type" is read.
    struct Section
    {
        int headerLine = -1;
        CodeLine header;
        QList<ParsedLine> lines;
    };
    QList<Section> sections;
    for (int n = 0; n < rawLines.size(); ++n) {
        QString lineText = rawLines[n];
        if (lineText.endsWith(QLatin1Char('\r')))
            lineText.chop(1);
        const CodeLine line = CodeLine::parse(lineText);
        if (line.kind == CodeLine::Kind::Section) {
            Section s;
            s.headerLine = n;
            s.header = line;
            sections << s;
            if (!line.headerClosed)
                issues << makeIssue(Issue::Severity::Error, n, line.headerStart, line.headerLength,
                                    SyntaxValidatorTexts::tr("Missing ']' in block header"));
        } else if (line.kind == CodeLine::Kind::KeyValue || line.kind == CodeLine::Kind::Raw) {
            if (!sections.isEmpty())
                sections.last().lines << ParsedLine{n, line};
        }
    }

    for (const Section& s : std::as_const(sections)) {
        const QString header = m_headerMapper ? m_headerMapper(s.header.header) : s.header.header;
        const bool typed = m_syntax.isTypedSection(m_fileType, header);
        QString typeValue;
        const ParsedLine* typeLine = nullptr;
        if (typed) {
            for (const ParsedLine& l : s.lines) {
                if (l.code.kind == CodeLine::Kind::KeyValue &&
                    l.code.key.compare(QLatin1String("type"), Qt::CaseInsensitive) == 0) {
                    typeValue = l.code.value;
                    typeLine = &l;
                    break;
                }
            }
            if (!typeLine && !m_syntax.hasPlainBlock(m_fileType, header)) {
                issues << makeIssue(Issue::Severity::Error, s.headerLine, s.header.headerStart, s.header.headerLength,
                                    SyntaxValidatorTexts::tr("Missing required parameter 'type'"));
                continue;
            }
        }

        const SyntaxBlock* block = m_syntax.findBlock(m_fileType, header, typeValue);
        if (!block) {
            if (typeLine)
                issues << makeIssue(Issue::Severity::Error, typeLine->number, typeLine->code.valueStart,
                                    int(typeLine->code.value.size()),
                                    SyntaxValidatorTexts::tr("Unknown type '%1'").arg(typeValue));
            else if (!isBuiltIn(header))
                issues << makeIssue(Issue::Severity::Error, s.headerLine, s.header.headerStart, s.header.headerLength,
                                    SyntaxValidatorTexts::tr("Unknown block header"));
            continue;
        }

        if (typeLine && block->typeName.isEmpty()) {
            if (!m_syntax.param(m_fileType, *block, QStringLiteral("type"))) {
                issues << makeIssue(Issue::Severity::Error, typeLine->number, typeLine->code.valueStart,
                                    int(typeLine->code.value.size()),
                                    SyntaxValidatorTexts::tr("Unknown type '%1'").arg(typeValue));
                continue;
            }
            typeLine = nullptr;
        }
        const QString blockName = block->typeName.isEmpty() ? block->section : block->typeName;
        if (!SyntaxDatabase::availableIn(block->minver, m_version))
            issues << makeIssue(Issue::Severity::Warning, s.headerLine, s.header.headerStart, s.header.headerLength,
                                SyntaxValidatorTexts::tr("Invalid block for current project version"));
        if (SyntaxDatabase::deprecatedIn(block->deprecated, m_version))
            issues << makeIssue(Issue::Severity::Deprecation, s.headerLine, s.header.headerStart, s.header.headerLength,
                                SyntaxValidatorTexts::tr("Block deprecated in %1").arg(block->deprecated));
        if (block->raw)
            continue;
        QSet<QString> seen;
        for (const ParsedLine& l : s.lines) {
            if (l.code.kind != CodeLine::Kind::KeyValue)
                continue;
            if (typeLine && &l == typeLine)
                continue;
            const QString key = l.code.key;
            if (!block->typeName.isEmpty() && key.compare(QLatin1String("type"), Qt::CaseInsensitive) == 0) {
                // The section's type was read from its first "type" line.
                issues << makeIssue(
                    Issue::Severity::Warning, l.number, l.code.keyStart, l.code.keyLength,
                    SyntaxValidatorTexts::tr("Duplicate parameter '%1' on group '%2'").arg(key, header));
                continue;
            }
            const SyntaxParam* param = m_syntax.param(m_fileType, *block, key);
            if (!param && typed) {
                for (const SyntaxParam& p : m_typedParams) {
                    if (SyntaxDatabase::wildcardMatch(p.name, key)) {
                        param = &p;
                        break;
                    }
                }
            }
            if (!param) {
                issues << makeIssue(
                    Issue::Severity::Error, l.number, l.code.keyStart, l.code.keyLength,
                    SyntaxValidatorTexts::tr("Unknown parameter '%1' for '%2' block").arg(key, blockName));
                continue;
            }
            bool repeatable = param->indexed;
            for (const QString& r : m_repeatable) {
                if (SyntaxDatabase::wildcardMatch(r, key)) {
                    repeatable = true;
                    break;
                }
            }
            const QString seenKey = key.toLower() + (param->indexed ? QLatin1Char('(') + l.code.index : QString());
            if (!repeatable && seen.contains(seenKey)) {
                issues << makeIssue(
                    Issue::Severity::Warning, l.number, l.code.keyStart, l.code.keyLength,
                    SyntaxValidatorTexts::tr("Duplicate parameter '%1' on group '%2'").arg(key, header));
            }
            seen.insert(seenKey);
            if (!SyntaxDatabase::availableIn(param->minver, m_version))
                issues << makeIssue(Issue::Severity::Warning, l.number, l.code.keyStart, l.code.keyLength,
                                    SyntaxValidatorTexts::tr("Invalid parameter for current project version"));
            if (SyntaxDatabase::deprecatedIn(param->deprecated, m_version))
                issues << makeIssue(Issue::Severity::Deprecation, l.number, l.code.keyStart, l.code.keyLength,
                                    SyntaxValidatorTexts::tr("Parameter deprecated in %1").arg(param->deprecated));
            checkValue(*param, l.code.value, l.code.valueStart, l.number, header, block->typeName, issues);
        }

        for (const SyntaxParam& p : m_syntax.params(m_fileType, *block)) {
            if (!p.required || p.name.contains(QLatin1Char('*')))
                continue;
            if (!seen.contains(p.name.toLower()))
                issues << makeIssue(Issue::Severity::Error, s.headerLine, s.header.headerStart, s.header.headerLength,
                                    SyntaxValidatorTexts::tr("Missing required parameter '%1'").arg(p.name));
        }
    }
    return issues;
}

} // namespace OFS
