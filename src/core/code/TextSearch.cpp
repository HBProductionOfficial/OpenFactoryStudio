#include "core/code/TextSearch.h"

#include "core/code/CodeEditor.h"

#include <QPlainTextEdit>
#include <QTextDocument>

namespace OFS {

namespace {

TextSearch::Match toMatch(const QRegularExpressionMatch& m)
{
    TextSearch::Match match;
    match.start = m.capturedStart();
    match.length = m.capturedLength();
    match.captures = m.capturedTexts();
    return match;
}

} // namespace

TextSearch::TextSearch(const QString& pattern, const SearchOptions& options)
    : m_pattern(pattern)
    , m_options(options)
{
    QString expression = options.regExp ? pattern : QRegularExpression::escape(pattern);
    if (options.wholeWords)
        expression = QStringLiteral("(?<!\\w)(?:%1)(?!\\w)").arg(expression);
    QRegularExpression::PatternOptions flags = QRegularExpression::MultilineOption;
    if (!options.caseSensitive)
        flags |= QRegularExpression::CaseInsensitiveOption;
    m_regex = QRegularExpression(expression, flags);
}

bool TextSearch::isValid() const
{
    return m_regex.isValid();
}

QString TextSearch::errorString() const
{
    return m_regex.errorString();
}

TextSearch::Match TextSearch::find(const QString& text, qsizetype from, bool backward) const
{
    if (m_pattern.isEmpty() || !m_regex.isValid())
        return {};
    if (!backward) {
        for (qsizetype offset = qBound<qsizetype>(0, from, text.size()); offset <= text.size();) {
            const QRegularExpressionMatch m = m_regex.match(text, offset);
            if (!m.hasMatch())
                return {};
            if (m.capturedLength() > 0)
                return toMatch(m);
            offset = m.capturedStart() + 1;
        }
        return {};
    }
    Match last;
    QRegularExpressionMatchIterator it = m_regex.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        if (m.capturedEnd() > from)
            break;
        if (m.capturedLength() > 0)
            last = toMatch(m);
    }
    return last;
}

QList<TextSearch::Match> TextSearch::findAll(const QString& text) const
{
    QList<Match> matches;
    if (m_pattern.isEmpty() || !m_regex.isValid())
        return matches;
    QRegularExpressionMatchIterator it = m_regex.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        if (m.capturedLength() > 0)
            matches << toMatch(m);
    }
    return matches;
}

TextSearch::Match TextSearch::matchAt(const QString& text, qsizetype start, qsizetype length) const
{
    if (m_pattern.isEmpty() || !m_regex.isValid() || length <= 0)
        return {};
    const QRegularExpressionMatch m =
        m_regex.match(text, start, QRegularExpression::NormalMatch, QRegularExpression::AnchorAtOffsetMatchOption);
    if (!m.hasMatch() || m.capturedStart() != start || m.capturedLength() != length)
        return {};
    return toMatch(m);
}

QString TextSearch::replacement(const Match& match, const QString& replace) const
{
    if (!m_options.regExp)
        return replace;
    QString result;
    result.reserve(replace.size());
    for (qsizetype i = 0; i < replace.size(); ++i) {
        const QChar c = replace.at(i);
        if (c == QLatin1Char('%') && i + 1 < replace.size() && replace.at(i + 1).isDigit()) {
            const int index = replace.at(i + 1).digitValue();
            if (index < match.captures.size())
                result += match.captures.at(index);
            ++i;
        } else {
            result += c;
        }
    }
    return result;
}

namespace TextSearchOps {

namespace {

void select(QPlainTextEdit* edit, const TextSearch::Match& match)
{
    if (auto* code = qobject_cast<CodeEditor*>(edit))
        code->revealPosition(int(match.start));
    QTextCursor cursor(edit->document());
    cursor.setPosition(int(match.start));
    cursor.setPosition(int(match.start + match.length), QTextCursor::KeepAnchor);
    edit->setTextCursor(cursor);
    edit->ensureCursorVisible();
}

} // namespace

bool find(QPlainTextEdit* edit, const TextSearch& search, bool backward, bool wrap)
{
    if (!edit || search.isEmpty() || !search.isValid())
        return false;
    const QString text = edit->document()->toPlainText();
    const QTextCursor cursor = edit->textCursor();
    TextSearch::Match match = search.find(text, backward ? cursor.selectionStart() : cursor.selectionEnd(), backward);
    if (!match.isValid() && wrap)
        match = search.find(text, backward ? text.size() : 0, backward);
    if (!match.isValid())
        return false;
    select(edit, match);
    return true;
}

bool replace(QPlainTextEdit* edit, const TextSearch& search, const QString& replace, bool backward, bool wrap)
{
    if (!edit || edit->isReadOnly() || search.isEmpty() || !search.isValid())
        return false;
    bool replaced = false;
    QTextCursor cursor = edit->textCursor();
    if (cursor.hasSelection()) {
        const QString text = edit->document()->toPlainText();
        const TextSearch::Match match =
            search.matchAt(text, cursor.selectionStart(), cursor.selectionEnd() - cursor.selectionStart());
        if (match.isValid()) {
            cursor.insertText(search.replacement(match, replace));
            if (backward)
                cursor.setPosition(int(match.start));
            edit->setTextCursor(cursor);
            replaced = true;
        }
    }
    const bool found = find(edit, search, backward, wrap);
    return replaced || found;
}

int replaceAll(QPlainTextEdit* edit, const TextSearch& search, const QString& replace)
{
    if (!edit || edit->isReadOnly())
        return 0;
    const QList<TextSearch::Match> matches = search.findAll(edit->document()->toPlainText());
    if (matches.isEmpty())
        return 0;
    QTextCursor cursor(edit->document());
    cursor.beginEditBlock();
    for (auto it = matches.crbegin(); it != matches.crend(); ++it) {
        cursor.setPosition(int(it->start));
        cursor.setPosition(int(it->start + it->length), QTextCursor::KeepAnchor);
        cursor.insertText(search.replacement(*it, replace));
    }
    cursor.endEditBlock();
    return int(matches.size());
}

} // namespace TextSearchOps

} // namespace OFS
