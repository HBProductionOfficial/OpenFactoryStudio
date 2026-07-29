#include "core/code/CodeCompletion.h"

#include "core/code/CodeLine.h"
#include "core/code/CompletionModels.h"
#include "core/syntax/SyntaxDatabase.h"

#include <QRegularExpression>
#include <QTextBlock>

#include <algorithm>

namespace OFS {

CompletionProvider::~CompletionProvider() = default;

namespace {

bool isWordChar(QChar c)
{
    return c.isLetterOrNumber() || c == QLatin1Char('_') || c == QLatin1Char('.');
}

QStringList sortedWords(QStringList words)
{
    words.removeDuplicates();
    std::sort(words.begin(), words.end(),
              [](const QString& a, const QString& b) { return QString::compare(a, b, Qt::CaseInsensitive) < 0; });
    return words;
}

QString withoutSpaces(const QString& text)
{
    QString result;
    for (QChar c : text) {
        if (!c.isSpace())
            result += c;
    }
    return result;
}

int topLevelCommas(const QString& text, qsizetype* lastComma)
{
    int depth = 0;
    int count = 0;
    bool quoted = false;
    *lastComma = -1;
    for (qsizetype i = 0; i < text.size(); ++i) {
        const QChar c = text.at(i);
        if (c == QLatin1Char('"'))
            quoted = !quoted;
        else if (quoted)
            continue;
        else if (c == QLatin1Char('(') || c == QLatin1Char('['))
            ++depth;
        else if (c == QLatin1Char(')') || c == QLatin1Char(']'))
            --depth;
        else if (c == QLatin1Char(',') && depth <= 0) {
            ++count;
            *lastComma = i;
        }
    }
    return count;
}

bool isBuiltInExpressionKey(const QString& key)
{
    static const QRegularExpression trigger(QStringLiteral("^trigger(all|\\d+)$"),
                                            QRegularExpression::CaseInsensitiveOption);
    return trigger.match(key).hasMatch() || key.compare(QLatin1String("persistent"), Qt::CaseInsensitive) == 0 ||
           key.compare(QLatin1String("ignorehitpause"), Qt::CaseInsensitive) == 0;
}

} // namespace

SyntaxCompletionProvider::SyntaxCompletionProvider(const SyntaxDatabase* syntax, const QString& fileType,
                                                   const CompletionModels* models)
    : m_syntax(syntax)
    , m_fileType(fileType)
    , m_models(models)
{}

CompletionRequest SyntaxCompletionProvider::complete(const QTextBlock& block, int column) const
{
    if (!m_syntax || m_fileType.isEmpty())
        return {};
    const QString text = block.text();
    const CodeLine line = CodeLine::parse(text);
    if (line.kind != CodeLine::Kind::KeyValue || line.separator < 0 || column <= line.separator ||
        column > text.size() || (line.commentStart >= 0 && column > line.commentStart))
        return {};
    const QString valueBefore = text.mid(line.separator + 1, column - line.separator - 1);
    if (valueBefore.count(QLatin1Char('"')) % 2 == 1)
        return {};

    QString header;
    QString typeValue;
    QTextBlock headerBlock;
    for (QTextBlock b = block; b.isValid(); b = b.previous()) {
        const CodeLine l = CodeLine::parse(b.text());
        if (l.kind == CodeLine::Kind::Section) {
            header = l.header;
            headerBlock = b;
            break;
        }
    }
    if (header.isEmpty())
        return {};
    for (QTextBlock b = headerBlock.next(); b.isValid(); b = b.next()) {
        const CodeLine l = CodeLine::parse(b.text());
        if (l.kind == CodeLine::Kind::Section)
            break;
        if (l.kind == CodeLine::Kind::KeyValue && l.key.compare(QLatin1String("type"), Qt::CaseInsensitive) == 0) {
            typeValue = l.value;
            break;
        }
    }
    const SyntaxBlock* syntaxBlock = m_syntax->findBlock(m_fileType, header, typeValue);
    const SyntaxParam* param = syntaxBlock ? m_syntax->param(m_fileType, *syntaxBlock, line.key) : nullptr;
    const QString paramType = param ? param->type.toLower() : QString();
    const int caret = block.position() + column;

    CompletionRequest request;
    // Right after "=": the section types for "type", or the keywords of a keyword parameter.
    if (valueBefore.trimmed().isEmpty()) {
        if (line.key.compare(QLatin1String("type"), Qt::CaseInsensitive) == 0 &&
            m_syntax->isTypedSection(m_fileType, header)) {
            request.words = sortedWords(m_syntax->sectionTypes(m_fileType, header));
        } else if (paramType == QLatin1String("keyword") && !param->list.isEmpty()) {
            request.words = sortedWords(param->list);
        }
        if (!request.words.isEmpty())
            request.start = caret;
        return request;
    }

    // The keyword that follows the expressions of an expkeyword value ("priority = 3, ").
    if ((paramType == QLatin1String("expkeyword") || paramType == QLatin1String("exp2keyword")) &&
        !param->list.isEmpty()) {
        qsizetype lastComma = -1;
        const int commas = topLevelCommas(valueBefore, &lastComma);
        const int expressions = paramType == QLatin1String("expkeyword") ? 1 : 2;
        const QString afterComma = valueBefore.mid(lastComma + 1);
        if (commas == expressions && !afterComma.isEmpty() && afterComma.trimmed().isEmpty()) {
            request.words = sortedWords(param->list);
            request.start = caret;
            return request;
        }
    }

    int wordStart = column;
    while (wordStart > line.separator + 1 && isWordChar(text.at(wordStart - 1)))
        --wordStart;
    const QString word = text.mid(wordStart, column - wordStart);
    const QString beforeWord = withoutSpaces(text.mid(line.separator + 1, wordStart - line.separator - 1));

    // "const(" and the other "(" models: the constants, inserted without the closing parenthesis.
    if (m_models) {
        for (const CompletionModels::Model& model : m_models->models()) {
            const QString prefix = withoutSpaces(model.prefix);
            if (!prefix.endsWith(QLatin1Char('(')) || !beforeWord.endsWith(prefix, Qt::CaseInsensitive))
                continue;
            // The prefix must start a word: "const(" matches, "myconst(" does not.
            const qsizetype at = beforeWord.size() - prefix.size();
            if (at > 0 && isWordChar(beforeWord.at(at - 1)))
                continue;
            QStringList words;
            for (QString w : model.words) {
                w = w.trimmed();
                if (w.endsWith(QLatin1Char(')')))
                    w.chop(1);
                words << w;
            }
            request.words = sortedWords(words);
            request.start = block.position() + wordStart;
            return request;
        }
    }

    // Trigger names, from the second letter of a word in an expression.
    const bool expression = paramType.startsWith(QLatin1String("exp")) ||
                            (m_syntax->isTypedSection(m_fileType, header) && isBuiltInExpressionKey(line.key));
    if (expression && word.size() >= 2 && word.at(0).isLetter() && !word.contains(QLatin1Char('.')) &&
        (wordStart == 0 || text.at(wordStart - 1) != QLatin1Char('.'))) {
        QStringList names;
        for (QString name : m_syntax->triggerNames())
            names << name.remove(QLatin1Char('*'));
        request.words = sortedWords(names);
        request.start = block.position() + wordStart;
    }
    return request;
}

} // namespace OFS
