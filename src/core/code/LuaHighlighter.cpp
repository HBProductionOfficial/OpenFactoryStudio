#include "core/code/LuaHighlighter.h"

#include <QSet>

namespace OFS {

namespace {

// Block states: what a line ends inside, with the level of its long bracket ("[==[": 2).
constexpr int InLongComment = 0x100;
constexpr int InLongString = 0x200;

const QSet<QString>& keywords()
{
    static const QSet<QString> words = {
        QStringLiteral("and"),      QStringLiteral("break"),  QStringLiteral("do"),    QStringLiteral("else"),
        QStringLiteral("elseif"),   QStringLiteral("end"),    QStringLiteral("false"), QStringLiteral("for"),
        QStringLiteral("function"), QStringLiteral("goto"),   QStringLiteral("if"),    QStringLiteral("in"),
        QStringLiteral("local"),    QStringLiteral("nil"),    QStringLiteral("not"),   QStringLiteral("or"),
        QStringLiteral("repeat"),   QStringLiteral("return"), QStringLiteral("then"),  QStringLiteral("true"),
        QStringLiteral("until"),    QStringLiteral("while"),
    };
    return words;
}

const QSet<QString>& apiNames()
{
    static const QSet<QString> names = {
        QStringLiteral("FF"),          QStringLiteral("OFS"),   QStringLiteral("MUGEN"),  QStringLiteral("ScriptPath"),
        QStringLiteral("debugOutput"), QStringLiteral("print"), QStringLiteral("string"), QStringLiteral("table"),
        QStringLiteral("math"),        QStringLiteral("utf8"),  QStringLiteral("base"),
    };
    return names;
}

int bracketLevel(const QString& text, int i)
{
    if (i >= text.size() || text.at(i) != QLatin1Char('['))
        return -1;
    int j = i + 1;
    while (j < text.size() && text.at(j) == QLatin1Char('='))
        ++j;
    return j < text.size() && text.at(j) == QLatin1Char('[') ? j - i - 1 : -1;
}

int closeOf(const QString& text, int from, int level)
{
    const QString close = QLatin1Char(']') + QString(level, QLatin1Char('=')) + QLatin1Char(']');
    const int at = int(text.indexOf(close, from));
    return at < 0 ? -1 : at + int(close.size());
}

} // namespace

LuaHighlighter::LuaHighlighter(QTextDocument* document)
    : QSyntaxHighlighter(document)
{
    setColorSet(ColorSet());
}

void LuaHighlighter::setColorSet(const ColorSet& colors)
{
    m_keyword.setForeground(colors.color(QStringLiteral("keyword"), QColor(0x00, 0x00, 0xC0)));
    m_keyword.setFontWeight(QFont::Bold);
    m_number.setForeground(colors.color(QStringLiteral("number"), QColor(0xC0, 0x40, 0x00)));
    m_string.setForeground(colors.color(QStringLiteral("string"), QColor(0x80, 0x00, 0x80)));
    m_comment.setForeground(colors.color(QStringLiteral("comment"), QColor(0x00, 0x80, 0x00)));
    m_api.setForeground(colors.color(QStringLiteral("function"), QColor(0x00, 0x60, 0x90)));
    rehighlight();
}

void LuaHighlighter::highlightBlock(const QString& text)
{
    int i = 0;
    const int previous = previousBlockState() < 0 ? 0 : previousBlockState();
    setCurrentBlockState(0);
    if (previous & (InLongComment | InLongString)) {
        const int level = previous & 0xFF;
        const int end = closeOf(text, 0, level);
        const QTextCharFormat& format = (previous & InLongComment) ? m_comment : m_string;
        if (end < 0) {
            setFormat(0, int(text.size()), format);
            setCurrentBlockState(previous);
            return;
        }
        setFormat(0, end, format);
        i = end;
    }
    while (i < text.size()) {
        const QChar c = text.at(i);
        if (c == QLatin1Char('-') && i + 1 < text.size() && text.at(i + 1) == QLatin1Char('-')) {
            const int level = bracketLevel(text, i + 2);
            if (level >= 0) {
                const int end = closeOf(text, i + 2, level);
                if (end < 0) {
                    setFormat(i, int(text.size()) - i, m_comment);
                    setCurrentBlockState(InLongComment | level);
                    return;
                }
                setFormat(i, end - i, m_comment);
                i = end;
                continue;
            }
            setFormat(i, int(text.size()) - i, m_comment);
            return;
        }
        if (c == QLatin1Char('"') || c == QLatin1Char('\'')) {
            int j = i + 1;
            while (j < text.size() && text.at(j) != c)
                j += text.at(j) == QLatin1Char('\\') ? 2 : 1;
            j = std::min(int(text.size()), j + 1);
            setFormat(i, j - i, m_string);
            i = j;
            continue;
        }
        if (c == QLatin1Char('[')) {
            const int level = bracketLevel(text, i);
            if (level >= 0) {
                const int end = closeOf(text, i, level);
                if (end < 0) {
                    setFormat(i, int(text.size()) - i, m_string);
                    setCurrentBlockState(InLongString | level);
                    return;
                }
                setFormat(i, end - i, m_string);
                i = end;
                continue;
            }
        }
        if (c.isDigit() || (c == QLatin1Char('.') && i + 1 < text.size() && text.at(i + 1).isDigit())) {
            int j = i + 1;
            while (j < text.size() && (text.at(j).isLetterOrNumber() || text.at(j) == QLatin1Char('.')))
                ++j;
            setFormat(i, j - i, m_number);
            i = j;
            continue;
        }
        if (c.isLetter() || c == QLatin1Char('_')) {
            int j = i + 1;
            while (j < text.size() && (text.at(j).isLetterOrNumber() || text.at(j) == QLatin1Char('_')))
                ++j;
            const QString word = text.mid(i, j - i);
            if (keywords().contains(word))
                setFormat(i, j - i, m_keyword);
            else if (apiNames().contains(word))
                setFormat(i, j - i, m_api);
            i = j;
            continue;
        }
        ++i;
    }
}

} // namespace OFS
