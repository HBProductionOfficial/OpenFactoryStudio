#include "core/code/CodeFormatter.h"

#include "core/syntax/SyntaxDatabase.h"

#include <QList>
#include <QRegularExpression>

#include <algorithm>

namespace OFS::CodeFormatter {

namespace {

constexpr int kTabWidth = 4;

int roundUp(int column)
{
    return (column + kTabWidth - 1) / kTabWidth * kTabWidth;
}

int widthOf(const QString& text)
{
    int column = 0;
    for (QChar c : text)
        column = c == QLatin1Char('\t') ? (column / kTabWidth + 1) * kTabWidth : column + 1;
    return column;
}

QString tabsTo(int column, int target)
{
    QString tabs;
    while (column < target) {
        column = (column / kTabWidth + 1) * kTabWidth;
        tabs += QLatin1Char('\t');
    }
    return tabs;
}

QString rightTrimmed(const QString& text)
{
    qsizetype end = text.size();
    while (end > 0 && text.at(end - 1).isSpace())
        --end;
    return text.left(end);
}

qsizetype commentStart(const QString& text)
{
    bool quoted = false;
    for (qsizetype i = 0; i < text.size(); ++i) {
        if (text.at(i) == QLatin1Char('"'))
            quoted = !quoted;
        else if (!quoted && text.at(i) == QLatin1Char(';'))
            return i;
    }
    return -1;
}

bool isConstantSection(const QString& header)
{
    static const QStringList constants = {QStringLiteral("Data"), QStringLiteral("Size"), QStringLiteral("Velocity"),
                                          QStringLiteral("Movement")};
    return constants.contains(header.trimmed(), Qt::CaseInsensitive);
}

QString headerName(const QString& stripped)
{
    QString h = stripped.mid(1);
    const qsizetype close = h.indexOf(QLatin1Char(']'));
    return (close >= 0 ? h.left(close) : h).trimmed();
}

struct Aligned
{
    int index = 0;
    int group = 0; // 0: constant sections, 1: the others
    QString key;
    QString value;
    QString comment;
};

} // namespace

QString formatExpression(const QString& value)
{
    enum class Kind
    {
        String,
        Operator,
        Comma,
        Other
    };
    struct Token
    {
        Kind kind;
        QString text;
        bool spaceBefore;
    };
    static const QStringList operators = {
        QStringLiteral(":="), QStringLiteral("**"), QStringLiteral("&&"), QStringLiteral("||"), QStringLiteral("^^"),
        QStringLiteral("!="), QStringLiteral("<="), QStringLiteral(">="), QStringLiteral("="),  QStringLiteral("<"),
        QStringLiteral(">"),  QStringLiteral("+"),  QStringLiteral("-"),  QStringLiteral("*"),  QStringLiteral("/"),
        QStringLiteral("%"),  QStringLiteral("&"),  QStringLiteral("|"),  QStringLiteral("^")};
    auto isWordChar = [](QChar c) {
        return c.isLetterOrNumber() || c == QLatin1Char('_') || c == QLatin1Char('.');
    };

    QList<Token> tokens;
    bool space = false;
    const qsizetype n = value.size();
    for (qsizetype i = 0; i < n;) {
        const QChar c = value.at(i);
        if (c.isSpace()) {
            space = true;
            ++i;
            continue;
        }
        Token token{Kind::Other, {}, space};
        space = false;
        if (c == QLatin1Char('"')) {
            const qsizetype close = value.indexOf(QLatin1Char('"'), i + 1);
            const qsizetype end = close < 0 ? n : close + 1;
            token = {Kind::String, value.mid(i, end - i), token.spaceBefore};
            i = end;
        } else if (c == QLatin1Char(',')) {
            token.kind = Kind::Comma;
            token.text = c;
            ++i;
        } else if (c == QLatin1Char('-') && i + 1 < n && value.at(i + 1).isDigit()) {
            qsizetype end = i + 1;
            while (end < n && isWordChar(value.at(end)))
                ++end;
            token.text = value.mid(i, end - i);
            i = end;
        } else if (isWordChar(c)) {
            qsizetype end = i;
            while (end < n && isWordChar(value.at(end)))
                ++end;
            token.text = value.mid(i, end - i);
            i = end;
        } else {
            for (const QString& op : operators) {
                if (QStringView(value).mid(i).startsWith(op)) {
                    token.kind = Kind::Operator;
                    token.text = op;
                    break;
                }
            }
            if (token.text.isEmpty())
                token.text = c;
            i += token.text.size();
        }
        tokens << token;
    }

    QString out;
    for (qsizetype k = 0; k < tokens.size(); ++k) {
        const Token& t = tokens.at(k);
        if (k > 0) {
            const Token& prev = tokens.at(k - 1);
            if (t.kind == Kind::Comma)
                ;
            else if (prev.kind == Kind::Comma || t.kind == Kind::Operator || prev.kind == Kind::Operator)
                out += QLatin1Char(' ');
            else if (t.spaceBefore)
                out += QLatin1Char(' ');
        }
        out += t.text;
    }
    return out;
}

QStringList process(const QStringList& lines, int first, int last, Mode mode, const ReformatPredicate& reformat)
{
    first = qMax(0, first);
    last = qMin(last, int(lines.size()) - 1);
    QStringList out;
    QList<Aligned> aligned;
    bool afterHeader = false;
    int group = 1;
    QString header;
    QString typeValue;

    for (int n = first; n <= last; ++n) {
        const QString stripped = lines.at(n).trimmed();
        if (stripped.startsWith(QLatin1Char('['))) {
            out << stripped;
            afterHeader = true;
            header = headerName(stripped);
            group = isConstantSection(header) ? 0 : 1;
            // The section's type, which may be written below the processed lines.
            typeValue.clear();
            for (int m = n + 1; m < lines.size(); ++m) {
                const QString l = lines.at(m).trimmed();
                if (l.startsWith(QLatin1Char('[')))
                    break;
                const qsizetype eq = l.indexOf(QLatin1Char('='));
                if (eq > 0 && l.left(eq).trimmed().compare(QLatin1String("type"), Qt::CaseInsensitive) == 0) {
                    QString v = l.mid(eq + 1);
                    const qsizetype c = commentStart(v);
                    typeValue = (c >= 0 ? v.left(c) : v).trimmed();
                    break;
                }
            }
            continue;
        }
        const qsizetype eq = stripped.indexOf(QLatin1Char('='));
        const qsizetype semicolon = commentStart(stripped);
        if (!afterHeader || stripped.isEmpty() || eq < 0 || (semicolon >= 0 && semicolon < eq)) {
            out << stripped; // only stripped: before the first header, blank, comment or no '='
            continue;
        }
        Aligned a;
        a.index = int(out.size());
        a.group = group;
        a.key = stripped.left(eq).trimmed();
        QString rest = stripped.mid(eq + 1);
        const qsizetype c = commentStart(rest);
        if (c >= 0) {
            a.comment = rest.mid(c);
            rest = rest.left(c);
        }
        rest = rightTrimmed(rest);
        if (mode == Mode::Format) {
            int leading = 0;
            while (leading < rest.size() && rest.at(leading).isSpace())
                ++leading;
            QString body = rest.mid(leading);
            if (reformat && reformat(header, typeValue, a.key))
                body = formatExpression(body);
            // At most one leading space is dropped: none or one gives "= v", more give "=  v".
            a.value = body.isEmpty() ? QString() : (leading >= 2 ? QStringLiteral("  ") : QStringLiteral(" ")) + body;
        } else {
            a.value = rest;
        }
        aligned << a;
        out << QString();
    }

    int equals[2] = {0, 0};
    for (const Aligned& a : std::as_const(aligned))
        equals[a.group] = qMax(equals[a.group], roundUp(widthOf(a.key)));
    QStringList code;
    int codeEnd[2] = {0, 0};
    for (const Aligned& a : std::as_const(aligned)) {
        const QString text = a.key + tabsTo(widthOf(a.key), equals[a.group]) + QLatin1Char('=') + a.value;
        code << text;
        codeEnd[a.group] = qMax(codeEnd[a.group], widthOf(text));
    }
    for (qsizetype i = 0; i < aligned.size(); ++i) {
        const Aligned& a = aligned.at(i);
        QString text = code.at(i);
        if (!a.comment.isEmpty()) {
            const int column = roundUp(codeEnd[a.group] + 1) + kTabWidth;
            text += tabsTo(widthOf(text), column) + a.comment;
        }
        out[a.index] = text;
    }
    return out;
}

ReformatPredicate syntaxPredicate(const SyntaxDatabase* syntax, const QString& fileType)
{
    return [syntax, fileType](const QString& header, const QString& typeValue, const QString& key) {
        if (!syntax)
            return false;
        const QString name = key.section(QLatin1Char('('), 0, 0).trimmed();
        // The trigger lines and other built-in expressions of controllers, whatever the
        // controller's own parameters say (Null takes any key as text).
        static const QRegularExpression builtIn(QStringLiteral("^(trigger(all|\\d+)|persistent|ignorehitpause)$"),
                                                QRegularExpression::CaseInsensitiveOption);
        if (syntax->isTypedSection(fileType, header) && builtIn.match(name).hasMatch())
            return true;
        if (const SyntaxBlock* block = syntax->findBlock(fileType, header, typeValue)) {
            if (const SyntaxParam* param = syntax->param(fileType, *block, name)) {
                const QString type = param->type.toLower();
                return type.startsWith(QLatin1String("exp")) || type == QLatin1String("hitattr") ||
                       type == QLatin1String("hitflags") || type == QLatin1String("guardflags");
            }
        }
        return false;
    };
}

} // namespace OFS::CodeFormatter
