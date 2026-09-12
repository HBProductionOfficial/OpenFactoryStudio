#include "engines/mugen/expr/Expression.h"

#include "core/code/CodeLine.h"
#include "core/syntax/SyntaxDatabase.h"

#include <QCoreApplication>
#include <QHash>

namespace OFS::Mugen {

namespace {

struct ExpressionTexts
{
    Q_DECLARE_TR_FUNCTIONS(OFS::Mugen::ExpressionTexts)
};

int precedence(const QString& op)
{
    static const QHash<QString, int> table = {
        {QStringLiteral("||"), 1}, {QStringLiteral("^^"), 2}, {QStringLiteral("&&"), 3},  {QStringLiteral("|"), 4},
        {QStringLiteral("^"), 5},  {QStringLiteral("&"), 6},  {QStringLiteral(":="), 7},  {QStringLiteral("="), 8},
        {QStringLiteral("!="), 8}, {QStringLiteral("<"), 9},  {QStringLiteral("<="), 9},  {QStringLiteral(">"), 9},
        {QStringLiteral(">="), 9}, {QStringLiteral("+"), 10}, {QStringLiteral("-"), 10},  {QStringLiteral("*"), 11},
        {QStringLiteral("/"), 11}, {QStringLiteral("%"), 11}, {QStringLiteral("**"), 12},
    };
    return table.value(op, 0);
}

bool rightAssociative(const QString& op)
{
    return op == QLatin1String("**") || op == QLatin1String(":=");
}

const QStringList kRedirectsWithArgument = {QStringLiteral("helper"), QStringLiteral("target"), QStringLiteral("enemy"),
                                            QStringLiteral("enemynear"), QStringLiteral("playerid")};
const QStringList kRedirectsWithoutArgument = {QStringLiteral("parent"), QStringLiteral("root"),
                                               QStringLiteral("partner")};

bool isComparison(const QString& op)
{
    return op == QLatin1String("=") || op == QLatin1String("!=") || op == QLatin1String("<") ||
           op == QLatin1String("<=") || op == QLatin1String(">") || op == QLatin1String(">=");
}

ExprPtr makeNode(ExprNode::Kind kind, const QString& text, int start, int length)
{
    auto node = std::make_shared<ExprNode>();
    node->kind = kind;
    node->text = text;
    node->start = start;
    node->length = length;
    return node;
}

} // namespace

ExpressionParser::ExpressionParser(const SyntaxDatabase* syntax, const QString& version)
    : m_syntax(syntax)
    , m_version(version)
{}

QList<QPair<QString, int>> ExpressionParser::splitTopLevel(const QString& text, int offset)
{
    QList<QPair<QString, int>> parts;
    int depth = 0;
    bool quoted = false;
    int start = 0;
    for (int i = 0; i < text.size(); ++i) {
        const QChar c = text.at(i);
        if (c == QLatin1Char('"'))
            quoted = !quoted;
        if (quoted)
            continue;
        if (c == QLatin1Char('(') || c == QLatin1Char('['))
            ++depth;
        else if (c == QLatin1Char(')') || c == QLatin1Char(']'))
            --depth;
        else if (c == QLatin1Char(',') && depth == 0) {
            parts.append({text.mid(start, i - start), offset + start});
            start = i + 1;
        }
    }
    parts.append({text.mid(start), offset + start});
    return parts;
}

const ExpressionParser::Token& ExpressionParser::peek(int ahead) const
{
    static const Token end;
    const int i = m_pos + ahead;
    return i < m_tokens.size() ? m_tokens[i] : end;
}

ExpressionParser::Token ExpressionParser::next()
{
    if (m_pos < m_tokens.size())
        return m_tokens[m_pos++];
    Token end;
    end.start = m_offset + int(m_text.size());
    return end;
}

bool ExpressionParser::atEnd() const
{
    return m_pos >= m_tokens.size();
}

bool ExpressionParser::accept(const QString& text)
{
    if (!atEnd() && peek().text == text) {
        ++m_pos;
        return true;
    }
    return false;
}

void ExpressionParser::error(int start, int length, const QString& message)
{
    if (m_failed)
        return; // one syntax error per expression
    m_failed = true;
    Issue issue;
    issue.severity = Issue::Severity::Error;
    issue.line = m_line;
    issue.column = start;
    issue.length = qMax(1, length);
    issue.message = message;
    m_issues->append(issue);
}

ExprPtr ExpressionParser::parse(const QString& text, int offset, int line, IssueList& issues)
{
    m_text = text;
    m_offset = offset;
    m_line = line;
    m_issues = &issues;
    m_failed = false;
    m_pos = 0;
    m_tokens.clear();

    for (const CodeToken& t : CodeLine::tokenize(text, offset)) {
        Token token;
        token.start = t.start;
        token.length = t.length;
        token.text = text.mid(t.start - offset, t.length);
        switch (t.kind) {
        case CodeToken::Kind::Number:
            token.kind = Token::Kind::Number;
            break;
        case CodeToken::Kind::String:
            token.kind = Token::Kind::String;
            if (token.text.size() < 2 || !token.text.endsWith(QLatin1Char('"')))
                error(token.start, token.length, ExpressionTexts::tr("Unterminated string"));
            break;
        case CodeToken::Kind::Identifier:
            token.kind = Token::Kind::Identifier;
            break;
        case CodeToken::Kind::Comma:
            token.kind = Token::Kind::Comma;
            break;
        case CodeToken::Kind::OpenParen:
            token.kind = Token::Kind::OpenParen;
            break;
        case CodeToken::Kind::CloseParen:
            token.kind = Token::Kind::CloseParen;
            break;
        case CodeToken::Kind::OpenBracket:
            token.kind = Token::Kind::OpenBracket;
            break;
        case CodeToken::Kind::CloseBracket:
            token.kind = Token::Kind::CloseBracket;
            break;
        case CodeToken::Kind::Operator:
            token.kind = Token::Kind::Operator;
            break;
        case CodeToken::Kind::Other:
            error(token.start, token.length, ExpressionTexts::tr("Syntax error - unexpected '%1'").arg(token.text));
            token.kind = Token::Kind::Operator;
            break;
        }
        m_tokens << token;
    }
    if (m_tokens.isEmpty()) {
        error(offset, 1, ExpressionTexts::tr("Syntax error - missing expression"));
        return nullptr;
    }

    ExprPtr root = parseExpression(1);
    if (!atEnd()) {
        const Token& t = peek();
        error(t.start, t.length, ExpressionTexts::tr("Syntax error - unexpected '%1'").arg(t.text));
    }
    return root;
}

ExprPtr ExpressionParser::parseExpression(int minPrecedence)
{
    ExprPtr left = parseUnary();
    while (!atEnd() && !m_failed) {
        const Token& op = peek();
        if (op.kind != Token::Kind::Operator)
            break;
        const int prec = precedence(op.text);
        if (prec == 0 || prec < minPrecedence)
            break;
        const Token opToken = next();
        // "x = [a, b]" and "x != (a, b)" are interval comparisons.
        if ((opToken.text == QLatin1String("=") || opToken.text == QLatin1String("!=")) &&
            (peek().kind == Token::Kind::OpenBracket || peek().kind == Token::Kind::OpenParen)) {
            int depth = 0;
            bool interval = false;
            for (int i = m_pos; i < m_tokens.size(); ++i) {
                const Token& t = m_tokens[i];
                if (t.kind == Token::Kind::OpenBracket || t.kind == Token::Kind::OpenParen)
                    ++depth;
                else if (t.kind == Token::Kind::CloseBracket || t.kind == Token::Kind::CloseParen) {
                    if (--depth == 0)
                        break;
                } else if (t.kind == Token::Kind::Comma && depth == 1) {
                    interval = true;
                    break;
                }
            }
            if (interval) {
                left = parseComparisonTail(left, opToken);
                continue;
            }
        }
        ExprPtr right = parseExpression(rightAssociative(opToken.text) ? prec : prec + 1);
        auto node = makeNode(ExprNode::Kind::Binary, opToken.text, opToken.start, opToken.length);
        node->children << left << right;
        left = node;
    }
    return left;
}

ExprPtr ExpressionParser::parseComparisonTail(ExprPtr left, const Token& op)
{
    const Token open = next();
    ExprPtr low = parseExpression(1);
    if (!accept(QStringLiteral(","))) {
        const Token& t = peek();
        error(t.start, t.length, ExpressionTexts::tr("Syntax error - ',' expected in interval"));
        return left;
    }
    ExprPtr high = parseExpression(1);
    const Token& close = peek();
    if (close.kind != Token::Kind::CloseBracket && close.kind != Token::Kind::CloseParen) {
        error(close.start, close.length, ExpressionTexts::tr("Syntax error - ']' or ')' expected"));
        return left;
    }
    next();
    auto node = makeNode(ExprNode::Kind::Interval, op.text + open.text + close.text, op.start, op.length);
    node->children << left << low << high;
    return node;
}

ExprPtr ExpressionParser::parseUnary()
{
    const Token& t = peek();
    if (t.kind == Token::Kind::Operator && (t.text == QLatin1String("!") || t.text == QLatin1String("~") ||
                                            t.text == QLatin1String("-") || t.text == QLatin1String("+"))) {
        const Token op = next();
        ExprPtr operand = parseUnary();
        auto node = makeNode(ExprNode::Kind::Unary, op.text, op.start, op.length);
        node->children << operand;
        return node;
    }
    return parsePrimary();
}

ExprPtr ExpressionParser::parsePrimary()
{
    if (atEnd()) {
        error(m_offset + int(m_text.size()), 1, ExpressionTexts::tr("Syntax error - code ends before line"));
        return nullptr;
    }
    const Token t = next();
    switch (t.kind) {
    case Token::Kind::Number:
        return makeNode(ExprNode::Kind::Number, t.text, t.start, t.length);
    case Token::Kind::String:
        return makeNode(ExprNode::Kind::String, t.text, t.start, t.length);
    case Token::Kind::OpenParen: {
        ExprPtr inner = parseExpression(1);
        if (!accept(QStringLiteral(")"))) {
            const Token& n = peek();
            error(atEnd() ? t.start : n.start, atEnd() ? 1 : n.length, ExpressionTexts::tr("Unbalanced parentheses"));
        }
        return inner;
    }
    case Token::Kind::Identifier:
        return parseTrigger(t);
    default:
        error(t.start, t.length, ExpressionTexts::tr("Syntax error - unexpected '%1'").arg(t.text));
        return nullptr;
    }
}

ExprPtr ExpressionParser::parseTrigger(const Token& name)
{
    const QString lower = name.text.toLower();

    // Redirections: "parent, Var(1)", "helper(2000), StateNo", "enemynear(1), Pos X".
    const bool redirectWithArg = kRedirectsWithArgument.contains(lower);
    if (redirectWithArg || kRedirectsWithoutArgument.contains(lower)) {
        auto node = makeNode(ExprNode::Kind::Redirect, name.text, name.start, name.length);
        if (redirectWithArg && peek().kind == Token::Kind::OpenParen) {
            next();
            node->children << parseExpression(1);
            if (!accept(QStringLiteral(")")))
                error(name.start, name.length, ExpressionTexts::tr("Unbalanced parentheses"));
        }
        if (peek().kind != Token::Kind::Comma) {
            error(name.start, name.length,
                  ExpressionTexts::tr("Syntax error - ',' expected after redirection '%1'").arg(name.text));
            return node;
        }
        next();
        const Token target = next();
        if (target.kind != Token::Kind::Identifier) {
            error(target.start, target.length,
                  ExpressionTexts::tr("Syntax error - trigger expected after redirection"));
            return node;
        }
        node->children << parseTrigger(target);
        return node;
    }

    const SyntaxTrigger* trigger = m_syntax ? m_syntax->trigger(name.text) : nullptr;
    if (!trigger) {
        if (m_syntax && !m_syntax->triggerNames().isEmpty())
            error(name.start, name.length, ExpressionTexts::tr("Unknown trigger '%1'").arg(name.text));
        // Skip a parenthesized argument list so one unknown name gives one error.
        if (peek().kind == Token::Kind::OpenParen) {
            int depth = 0;
            do {
                const Token t = next();
                if (t.kind == Token::Kind::OpenParen)
                    ++depth;
                else if (t.kind == Token::Kind::CloseParen)
                    --depth;
            } while (depth > 0 && !atEnd());
        }
        return makeNode(ExprNode::Kind::Trigger, name.text, name.start, name.length);
    }

    if (!SyntaxDatabase::availableIn(trigger->minver, m_version)) {
        Issue issue;
        issue.severity = Issue::Severity::Error;
        issue.line = m_line;
        issue.column = name.start;
        issue.length = name.length;
        issue.message = ExpressionTexts::tr("Trigger '%1' isn't valid for the current project version").arg(name.text);
        m_issues->append(issue);
    }
    if (SyntaxDatabase::deprecatedIn(trigger->deprecated, m_version)) {
        Issue issue;
        issue.severity = Issue::Severity::Deprecation;
        issue.line = m_line;
        issue.column = name.start;
        issue.length = name.length;
        issue.message = ExpressionTexts::tr("Trigger deprecated in %1").arg(trigger->deprecated);
        m_issues->append(issue);
    }

    const QString type = trigger->type.toLower();
    ExprPtr node = makeNode(ExprNode::Kind::Trigger, name.text, name.start, name.length);
    if (parseSpecialForm(type, name, node))
        return node;

    auto parseArguments = [&](int minCount, int maxCount, bool required) {
        if (peek().kind != Token::Kind::OpenParen) {
            if (required)
                error(name.start, name.length, ExpressionTexts::tr("Trigger '%1' needs an argument").arg(name.text));
            return;
        }
        next();
        int count = 0;
        if (peek().kind != Token::Kind::CloseParen) {
            do {
                node->children << parseExpression(1);
                ++count;
            } while (!m_failed && accept(QStringLiteral(",")));
        }
        if (!accept(QStringLiteral(")"))) {
            const Token& t = peek();
            error(atEnd() ? name.start : t.start, atEnd() ? name.length : t.length,
                  ExpressionTexts::tr("Unbalanced parentheses"));
            return;
        }
        if (count < minCount || count > maxCount)
            error(name.start, name.length,
                  ExpressionTexts::tr("Wrong number of arguments for trigger '%1'").arg(name.text));
    };

    if (type == QLatin1String("exp")) {
        parseArguments(1, 1, true);
    } else if (type == QLatin1String("exp2")) {
        parseArguments(2, 2, true);
    } else if (type == QLatin1String("exp3")) {
        parseArguments(3, 3, true);
    } else if (type == QLatin1String("nilexp") || type == QLatin1String("expopt")) {
        parseArguments(1, 1, false);
    } else if (type == QLatin1String("const") || type == QLatin1String("constexp")) {
        if (peek().kind != Token::Kind::OpenParen) {
            error(name.start, name.length, ExpressionTexts::tr("Trigger '%1' needs an argument").arg(name.text));
            return node;
        }
        next();
        const Token arg = next();
        if (arg.kind != Token::Kind::Identifier) {
            error(arg.start, arg.length, ExpressionTexts::tr("Syntax error - constant name expected"));
            return node;
        }
        QString group = trigger->name;
        if (m_syntax && !m_syntax->constNames(group).isEmpty() && !m_syntax->isConst(group, arg.text))
            error(arg.start, arg.length, ExpressionTexts::tr("Unknown constant '%1'").arg(arg.text));
        node->children << makeNode(ExprNode::Kind::Keyword, arg.text, arg.start, arg.length);
        if (!accept(QStringLiteral(")")))
            error(name.start, name.length, ExpressionTexts::tr("Unbalanced parentheses"));
    } else if (type == QLatin1String("keyword")) {
        const Token& component = peek();
        bool ok = component.kind == Token::Kind::Identifier;
        if (ok && !trigger->list.isEmpty()) {
            ok = false;
            for (const QString& k : trigger->list) {
                if (k.compare(component.text, Qt::CaseInsensitive) == 0) {
                    ok = true;
                    break;
                }
            }
        }
        if (!ok) {
            error(name.start, name.length,
                  ExpressionTexts::tr("Trigger '%1' needs one of: %2")
                      .arg(name.text, trigger->list.join(QStringLiteral(", "))));
            return node;
        }
        const Token k = next();
        node->children << makeNode(ExprNode::Kind::Keyword, k.text, k.start, k.length);
    } else if (peek().kind == Token::Kind::OpenParen && type.isEmpty()) {
        error(name.start, name.length, ExpressionTexts::tr("Trigger '%1' does not take arguments").arg(name.text));
    }
    return node;
}

bool ExpressionParser::parseSpecialForm(const QString& type, const Token& name, ExprPtr& node)
{
    // These triggers carry their own comparison: "AnimElem = 3, >= 2", "Command = \"x\"",
    // "StateType = S", "TimeMod = 10, 0", "HitDefAttr = SC, NA, SA", "ProjContact = 1".
    static const QStringList special = {QStringLiteral("opstr"),   QStringLiteral("opint"),
                                        QStringLiteral("opint2"),  QStringLiteral("op2int2"),
                                        QStringLiteral("valueeq"), QStringLiteral("hitattr")};
    if (!special.contains(type))
        return false;

    node->kind = ExprNode::Kind::Special;
    const Token& opToken = peek();
    const bool comparison = opToken.kind == Token::Kind::Operator && isComparison(opToken.text);
    const bool equality = comparison && (opToken.text == QLatin1String("=") || opToken.text == QLatin1String("!="));
    if (!comparison || (!equality && type != QLatin1String("opint2") && type != QLatin1String("op2int2") &&
                        type != QLatin1String("opint"))) {
        error(name.start, name.length,
              ExpressionTexts::tr("Trigger '%1' must be compared with '=' or '!='").arg(name.text));
        return true;
    }
    next();
    node->text = name.text + opToken.text;

    if (type == QLatin1String("opstr")) {
        const Token value = next();
        if (value.kind != Token::Kind::String)
            error(value.start, value.length, ExpressionTexts::tr("Syntax error - string expected"));
        else
            node->children << makeNode(ExprNode::Kind::String, value.text, value.start, value.length);
        return true;
    }
    if (type == QLatin1String("valueeq")) {
        const Token value = next();
        bool ok = value.kind == Token::Kind::Identifier;
        if (ok && m_syntax) {
            if (const SyntaxTrigger* t = m_syntax->trigger(name.text); t && !t->list.isEmpty()) {
                ok = false;
                for (const QString& k : t->list) {
                    if (k.compare(value.text, Qt::CaseInsensitive) == 0) {
                        ok = true;
                        break;
                    }
                }
            }
        }
        if (!ok)
            error(value.start, value.length, ExpressionTexts::tr("Unknown keyword '%1'").arg(value.text));
        else
            node->children << makeNode(ExprNode::Kind::Keyword, value.text, value.start, value.length);
        return true;
    }
    if (type == QLatin1String("hitattr")) {
        // State type letters, then two-letter attack classes, separated by commas.
        const Token first = next();
        if (first.kind != Token::Kind::Identifier) {
            error(first.start, first.length, ExpressionTexts::tr("Unknown hit attribute/flag"));
            return true;
        }
        for (QChar c : first.text) {
            if (!QStringLiteral("SCAscan").contains(c)) {
                error(first.start, first.length, ExpressionTexts::tr("Unknown hit attribute/flag"));
                return true;
            }
        }
        while (peek().kind == Token::Kind::Comma) {
            next();
            const Token cls = next();
            static const QStringList classes = {QStringLiteral("NA"), QStringLiteral("SA"), QStringLiteral("HA"),
                                                QStringLiteral("NP"), QStringLiteral("SP"), QStringLiteral("HP"),
                                                QStringLiteral("NT"), QStringLiteral("ST"), QStringLiteral("HT"),
                                                QStringLiteral("AA"), QStringLiteral("AT"), QStringLiteral("AP")};
            if (cls.kind != Token::Kind::Identifier || !classes.contains(cls.text.toUpper())) {
                error(cls.start, cls.length, ExpressionTexts::tr("Unknown hit attribute/flag"));
                return true;
            }
        }
        return true;
    }

    // Numeric forms: opint ("= v"), opint2 ("= div, v"), op2int2 ("= n" with optional ", [op] t").
    node->children << parseExpression(precedence(QStringLiteral("=")) + 1);
    if (type == QLatin1String("opint2")) {
        if (!accept(QStringLiteral(","))) {
            error(name.start, name.length, ExpressionTexts::tr("Syntax error - ',' expected"));
            return true;
        }
        node->children << parseExpression(precedence(QStringLiteral("=")) + 1);
    } else if ((type == QLatin1String("op2int2") || type == QLatin1String("opint")) &&
               peek().kind == Token::Kind::Comma) {
        next();
        if (peek().kind == Token::Kind::Operator && isComparison(peek().text))
            next();
        node->children << parseExpression(precedence(QStringLiteral("=")) + 1);
    }
    return true;
}

} // namespace OFS::Mugen
