#include "engines/mugen/MugenSyntax.h"

#include "core/code/CodeLine.h"
#include "engines/mugen/MugenTypes.h"
#include "engines/mugen/expr/Expression.h"

#include <QCoreApplication>
#include <QRegularExpression>

#include <algorithm>

namespace OFS::Mugen {

namespace {

struct MugenSyntaxTexts
{
    Q_DECLARE_TR_FUNCTIONS(OFS::Mugen::MugenSyntaxTexts)
};

void addIssue(IssueList& issues, int line, int column, int length, const QString& message,
              Issue::Severity severity = Issue::Severity::Error)
{
    Issue issue;
    issue.severity = severity;
    issue.line = line;
    issue.column = column;
    issue.length = length;
    issue.message = message;
    issues << issue;
}

int componentCount(const QString& type)
{
    static const QRegularExpression re(QStringLiteral("^exp(\\d)"));
    const auto m = re.match(type);
    return m.hasMatch() ? m.captured(1).toInt() : 0;
}

} // namespace

MugenExpressionChecker::MugenExpressionChecker(const SyntaxDatabase* syntax)
    : m_syntax(syntax)
{}

void MugenExpressionChecker::check(const QString& text, int offset, const Context& context, IssueList& issues) const
{
    const QString type = context.paramType.toLower();
    if (type == QLatin1String("hitattr")) {
        checkHitAttr(text, offset, context.line, issues);
        return;
    }
    if (type == QLatin1String("hitflags")) {
        checkFlags(text, offset, context.line, QStringLiteral("HLAFDMP+-"), issues);
        return;
    }
    if (type == QLatin1String("guardflags")) {
        checkFlags(text, offset, context.line, QStringLiteral("HLAM"), issues);
        return;
    }
    if (type == QLatin1String("keyseq")) {
        QString value = text.trimmed();
        if (value.startsWith(QLatin1Char('"')))
            value = value.mid(1);
        if (value.endsWith(QLatin1Char('"')))
            value.chop(1);
        QString problem;
        if (!isValidKeySequence(value, &problem))
            addIssue(issues, context.line, offset, int(text.size()), problem);
        return;
    }

    ExpressionParser parser(m_syntax, context.version);
    if (type == QLatin1String("exp") || type == QLatin1String("expi") || type == QLatin1String("expf") ||
        type == QLatin1String("expb")) {
        parser.parse(text, offset, context.line, issues);
        return;
    }

    const auto parts = ExpressionParser::splitTopLevel(text, offset);
    int expressions = int(parts.size());
    // expkeyword and exp2keyword: one or two expressions, then an optional keyword
    // ("priority = 4" or "priority = 4, Hit").
    if (type == QLatin1String("expkeyword") && parts.size() > 1)
        expressions = 1;
    else if (type == QLatin1String("exp2keyword") && parts.size() > 2)
        expressions = 2;
    int max = componentCount(type);
    if (type == QLatin1String("expkeyword"))
        max = 2;
    else if (type == QLatin1String("exp2keyword"))
        max = 3;
    if (max > 0 && parts.size() > max)
        addIssue(issues, context.line, offset, int(text.size()),
                 MugenSyntaxTexts::tr("Too many values (%1 at most)").arg(max), Issue::Severity::Warning);
    for (int i = 0; i < parts.size(); ++i) {
        const QString part = parts[i].first;
        if (part.trimmed().isEmpty())
            continue; // omitted component: the engine uses its default
        if (i >= expressions) {
            const QString word = part.trimmed();
            if (!context.list.isEmpty()) {
                if (!SyntaxDatabase::keywordMatches(context.list, word))
                    addIssue(issues, context.line, parts[i].second + int(part.indexOf(word)), int(word.size()),
                             MugenSyntaxTexts::tr("Unknown keyword '%1'").arg(word));
            }
            continue;
        }
        parser.parse(part, parts[i].second, context.line, issues);
    }
}

void MugenExpressionChecker::checkHitAttr(const QString& text, int offset, int line, IssueList& issues) const
{
    const auto parts = ExpressionParser::splitTopLevel(text, offset);
    const QString states = parts.value(0).first.trimmed();
    // No state letters is fine when attack classes follow (", NA, NP" in Kung Fu Man 1.0).
    if (states.isEmpty() && parts.size() < 2) {
        addIssue(issues, line, offset, int(text.size()), MugenSyntaxTexts::tr("Unknown hit attribute/flag"));
        return;
    }
    for (QChar c : states) {
        if (!QStringLiteral("SCAsca").contains(c)) {
            addIssue(issues, line, parts[0].second, int(parts[0].first.size()),
                     MugenSyntaxTexts::tr("Unknown hit attribute/flag"));
            return;
        }
    }
    static const QStringList classes = {QStringLiteral("NA"), QStringLiteral("SA"), QStringLiteral("HA"),
                                        QStringLiteral("NP"), QStringLiteral("SP"), QStringLiteral("HP"),
                                        QStringLiteral("NT"), QStringLiteral("ST"), QStringLiteral("HT"),
                                        QStringLiteral("AA"), QStringLiteral("AT"), QStringLiteral("AP")};
    for (int i = 1; i < parts.size(); ++i) {
        const QString cls = parts[i].first.trimmed();
        if (!classes.contains(cls.toUpper())) {
            addIssue(issues, line, parts[i].second + int(parts[i].first.indexOf(cls)), qMax(1, int(cls.size())),
                     MugenSyntaxTexts::tr("Unknown hit attribute/flag"));
            return;
        }
    }
}

void MugenExpressionChecker::checkFlags(const QString& text, int offset, int line, const QString& allowed,
                                        IssueList& issues) const
{
    const QString flags = text.trimmed();
    const int start = offset + int(text.indexOf(flags));
    for (QChar c : flags) {
        if (!allowed.contains(c.toUpper())) {
            addIssue(issues, line, start, int(flags.size()), MugenSyntaxTexts::tr("Unknown hit attribute/flag"));
            return;
        }
    }
    if (flags.contains(QLatin1Char('+')) && flags.contains(QLatin1Char('-')))
        addIssue(issues, line, start, int(flags.size()), MugenSyntaxTexts::tr("'+' and '-' are mutually exclusive"));
}

bool MugenExpressionChecker::isValidKeySequence(const QString& text, QString* problem)
{
    // Comma-separated elements. Each is an optional ">" (no other input between), "~" with an optional
    // hold time (release), "/" (hold) or "$" (4-way), then a direction or button, maybe joined with "+".
    static const QStringList directions = {QStringLiteral("B"),  QStringLiteral("DB"), QStringLiteral("D"),
                                           QStringLiteral("DF"), QStringLiteral("F"),  QStringLiteral("UF"),
                                           QStringLiteral("U"),  QStringLiteral("UB")};
    static const QStringList buttons = {QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c"),
                                        QStringLiteral("x"), QStringLiteral("y"), QStringLiteral("z"),
                                        QStringLiteral("s")};
    static const QRegularExpression element(
        QStringLiteral("^\\s*(>?)\\s*(~\\s*\\d*)?\\s*(/?)\\s*(\\$?)\\s*(.+?)\\s*$"));
    if (text.trimmed().isEmpty()) {
        if (problem)
            *problem = MugenSyntaxTexts::tr("Empty command");
        return false;
    }
    for (const QString& part : text.split(QLatin1Char(','))) {
        const auto m = element.match(part);
        if (!m.hasMatch()) {
            if (problem)
                *problem = MugenSyntaxTexts::tr("Invalid command element '%1'").arg(part.trimmed());
            return false;
        }
        for (const QString& key : m.captured(5).split(QLatin1Char('+'))) {
            const QString k = key.trimmed();
            const bool ok = directions.contains(k, Qt::CaseInsensitive) || buttons.contains(k, Qt::CaseInsensitive);
            if (!ok) {
                if (problem)
                    *problem = MugenSyntaxTexts::tr("Invalid command element '%1'").arg(part.trimmed());
                return false;
            }
        }
    }
    return true;
}

IssueList validateMugenCode(const SyntaxDatabase& syntax, const QString& fileType, const QString& version,
                            const QString& text)
{
    MugenExpressionChecker checker(&syntax);
    SyntaxValidator validator(syntax, fileType, version);
    validator.setExpressionChecker(&checker);
    validator.setRepeatableParams({QStringLiteral("trigger*"), QStringLiteral("triggerall")});
    validator.setBuiltInSections({QStringLiteral("Info"), QStringLiteral("Files"), QStringLiteral("Begin Action"),
                                  QStringLiteral("BGdef"), QStringLiteral("BG"), QStringLiteral("BGCtrlDef"),
                                  QStringLiteral("BGCtrl"), QStringLiteral("Characters"), QStringLiteral("ExtraStages"),
                                  QStringLiteral("*BGdef"), QStringLiteral("*BG"), QStringLiteral("*BGCtrlDef"),
                                  QStringLiteral("*BGCtrl")});
    QList<SyntaxParam> common;
    auto add = [&common](const char* name, const char* type) {
        SyntaxParam p;
        p.name = QString::fromLatin1(name);
        p.type = QString::fromLatin1(type);
        common << p;
    };
    add("triggerall", "expb");
    add("trigger*", "expb");
    add("persistent", "expi");
    add("ignorehitpause", "expb");
    validator.setTypedSectionParams(common);

    // A storyboard scene names its background with bg.name. "<name>Def", "<name> ...", "<name>CtrlDef"
    // and "<name>Ctrl ..." are its BG sections.
    if (fileType == MugenType::Storyboard) {
        QStringList names;
        static const QRegularExpression bgName(QStringLiteral("^\\s*bg\\.name\\s*=\\s*\"?([^\";\\r\\n]+)"),
                                               QRegularExpression::CaseInsensitiveOption |
                                                   QRegularExpression::MultilineOption);
        for (auto it = bgName.globalMatch(text); it.hasNext();) {
            const QString name = it.next().captured(1).trimmed();
            if (!name.isEmpty() && !names.contains(name, Qt::CaseInsensitive))
                names << name;
        }
        // Longer names first, so "Intro2" is not read as "Intro" followed by "2".
        std::sort(names.begin(), names.end(), [](const QString& a, const QString& b) { return a.size() > b.size(); });
        validator.setHeaderMapper([names](const QString& header) -> QString {
            const QString h = header.trimmed();
            for (const QString& name : names) {
                if (!h.startsWith(name, Qt::CaseInsensitive))
                    continue;
                const QString rest = h.mid(name.size());
                if (rest.isEmpty() || rest.at(0).isSpace() ||
                    rest.startsWith(QLatin1String("Def"), Qt::CaseInsensitive) ||
                    rest.startsWith(QLatin1String("Ctrl"), Qt::CaseInsensitive))
                    return QStringLiteral("BG") + rest;
            }
            return header;
        });
    }

    IssueList issues = validator.validate(text);

    // Every state controller needs a trigger1.
    if (fileType == QLatin1String("state") || fileType == QLatin1String("command")) {
        const QStringList lines = text.split(QLatin1Char('\n'));
        int header = -1;
        bool hasTrigger1 = true;
        auto flush = [&] {
            if (header >= 0 && !hasTrigger1)
                addIssue(issues, header, 0, int(lines[header].size()),
                         MugenSyntaxTexts::tr("Missing parameter 'trigger1'"));
        };
        for (int n = 0; n < lines.size(); ++n) {
            const CodeLine line = CodeLine::parse(lines[n]);
            if (line.kind == CodeLine::Kind::Section) {
                flush();
                const bool controller = line.header.startsWith(QLatin1String("State "), Qt::CaseInsensitive) ||
                                        line.header.compare(QLatin1String("State"), Qt::CaseInsensitive) == 0;
                header = controller ? n : -1;
                hasTrigger1 = !controller;
            } else if (line.kind == CodeLine::Kind::KeyValue &&
                       line.key.compare(QLatin1String("trigger1"), Qt::CaseInsensitive) == 0) {
                hasTrigger1 = true;
            }
        }
        flush();
    }
    return issues;
}

} // namespace OFS::Mugen
