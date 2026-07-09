#include "core/code/CodeLine.h"

namespace OFS {

namespace {

int findComment(const QString& text)
{
    bool quoted = false;
    for (int i = 0; i < text.size(); ++i) {
        const QChar c = text.at(i);
        if (c == QLatin1Char('"'))
            quoted = !quoted;
        else if (c == QLatin1Char(';') && !quoted)
            return i;
    }
    return -1;
}

int firstNonSpace(const QString& text, int from, int to)
{
    while (from < to && text.at(from).isSpace())
        ++from;
    return from;
}

int lastNonSpace(const QString& text, int from, int to)
{
    while (to > from && text.at(to - 1).isSpace())
        --to;
    return to;
}

bool isIdentStart(QChar c)
{
    return c.isLetter() || c == QLatin1Char('_') || c == QLatin1Char('$');
}

bool isIdentChar(QChar c)
{
    return c.isLetterOrNumber() || c == QLatin1Char('_') || c == QLatin1Char('.') || c == QLatin1Char('$');
}

} // namespace

QList<CodeToken> CodeLine::tokenize(const QString& text, int offset)
{
    QList<CodeToken> tokens;
    const int n = int(text.size());
    int i = 0;
    while (i < n) {
        const QChar c = text.at(i);
        if (c.isSpace()) {
            ++i;
            continue;
        }
        CodeToken t;
        t.start = offset + i;
        if (c == QLatin1Char('"')) {
            int j = i + 1;
            while (j < n && text.at(j) != QLatin1Char('"'))
                ++j;
            if (j < n)
                ++j;
            t.kind = CodeToken::Kind::String;
            t.length = j - i;
            i = j;
        } else if (c.isDigit() || (c == QLatin1Char('.') && i + 1 < n && text.at(i + 1).isDigit())) {
            int j = i;
            while (j < n && (text.at(j).isDigit() || text.at(j) == QLatin1Char('.')))
                ++j;
            t.kind = CodeToken::Kind::Number;
            t.length = j - i;
            i = j;
        } else if (isIdentStart(c)) {
            int j = i + 1;
            while (j < n && isIdentChar(text.at(j)))
                ++j;
            t.kind = CodeToken::Kind::Identifier;
            t.length = j - i;
            i = j;
        } else if (c == QLatin1Char(',')) {
            t.kind = CodeToken::Kind::Comma;
            t.length = 1;
            ++i;
        } else if (c == QLatin1Char('(')) {
            t.kind = CodeToken::Kind::OpenParen;
            t.length = 1;
            ++i;
        } else if (c == QLatin1Char(')')) {
            t.kind = CodeToken::Kind::CloseParen;
            t.length = 1;
            ++i;
        } else if (c == QLatin1Char('[')) {
            t.kind = CodeToken::Kind::OpenBracket;
            t.length = 1;
            ++i;
        } else if (c == QLatin1Char(']')) {
            t.kind = CodeToken::Kind::CloseBracket;
            t.length = 1;
            ++i;
        } else {
            static const QString twoChar[] = {QStringLiteral("**"), QStringLiteral("&&"), QStringLiteral("||"),
                                              QStringLiteral("^^"), QStringLiteral("!="), QStringLiteral("<="),
                                              QStringLiteral(">="), QStringLiteral(":=")};
            t.kind = CodeToken::Kind::Operator;
            t.length = 1;
            for (const QString& op : twoChar) {
                if (QStringView(text).mid(i, 2) == op) {
                    t.length = 2;
                    break;
                }
            }
            if (!QStringLiteral("+-*/%=<>!~&|^:").contains(c))
                t.kind = CodeToken::Kind::Other;
            i += t.length;
        }
        tokens << t;
    }
    return tokens;
}

CodeLine CodeLine::parse(const QString& text)
{
    CodeLine line;
    line.commentStart = findComment(text);
    const int end = line.commentStart >= 0 ? line.commentStart : int(text.size());
    const int start = firstNonSpace(text, 0, end);
    if (start >= end) {
        line.kind = line.commentStart >= 0 ? Kind::Comment : Kind::Blank;
        return line;
    }

    if (text.at(start) == QLatin1Char('[')) {
        line.kind = Kind::Section;
        int close = int(text.indexOf(QLatin1Char(']'), start + 1));
        if (close < 0 || close > end)
            close = -1;
        line.headerClosed = close >= 0;
        const int innerEnd = close >= 0 ? close : end;
        const int hs = firstNonSpace(text, start + 1, innerEnd);
        const int he = lastNonSpace(text, hs, innerEnd);
        line.headerStart = hs;
        line.headerLength = he - hs;
        line.header = text.mid(hs, he - hs);
        return line;
    }

    // key = value (the first '=' outside quotes); AIR also uses "key: value".
    int sep = -1;
    bool quoted = false;
    for (int i = start; i < end; ++i) {
        const QChar c = text.at(i);
        if (c == QLatin1Char('"'))
            quoted = !quoted;
        if (quoted)
            continue;
        if (c == QLatin1Char('=')) {
            sep = i;
            break;
        }
    }
    if (sep < 0) {
        const int colon = int(text.indexOf(QLatin1Char(':'), start));
        if (colon > start && colon < end) {
            bool ident = true;
            for (int i = start; i < colon; ++i) {
                if (!isIdentChar(text.at(i))) {
                    ident = false;
                    break;
                }
            }
            if (ident)
                sep = colon;
        }
    }

    if (sep > start) {
        line.kind = Kind::KeyValue;
        line.separator = sep;
        const int ke = lastNonSpace(text, start, sep);
        int keyEnd = ke;
        const int open = int(text.indexOf(QLatin1Char('('), start));
        const int openSq = int(text.indexOf(QLatin1Char('['), start));
        const int idxOpen = (open > start && open < ke) ? open : ((openSq > start && openSq < ke) ? openSq : -1);
        if (idxOpen > 0) {
            const QChar closeChar = text.at(idxOpen) == QLatin1Char('(') ? QLatin1Char(')') : QLatin1Char(']');
            const int idxClose = int(text.indexOf(closeChar, idxOpen + 1));
            if (idxClose > idxOpen && idxClose < ke + 1) {
                line.indexStart = idxOpen + 1;
                line.indexLength = idxClose - idxOpen - 1;
                line.index = text.mid(line.indexStart, line.indexLength).trimmed();
                keyEnd = lastNonSpace(text, start, idxOpen);
            }
        }
        line.keyStart = start;
        line.keyLength = keyEnd - start;
        line.key = text.mid(start, keyEnd - start);
        const int vs = firstNonSpace(text, sep + 1, end);
        const int ve = lastNonSpace(text, vs, end);
        line.valueStart = vs;
        line.value = text.mid(vs, ve - vs);
        line.tokens = tokenize(line.value, vs);
        return line;
    }

    line.kind = Kind::Raw;
    const int ve = lastNonSpace(text, start, end);
    line.valueStart = start;
    line.value = text.mid(start, ve - start);
    line.tokens = tokenize(line.value, start);
    return line;
}

} // namespace OFS
