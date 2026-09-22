#include "core/code/CodeHighlighter.h"

#include "core/code/CodeBlockData.h"
#include "core/code/CodeLine.h"
#include "core/syntax/SyntaxDatabase.h"

#include <QTextBlock>

namespace OFS {

namespace {

const QStringList kConstTriggers = {QStringLiteral("const"), QStringLiteral("gethitvar"), QStringLiteral("stagevar"),
                                    QStringLiteral("stageinfo")};

} // namespace

CodeHighlighter::CodeHighlighter(QTextDocument* document, const SyntaxDatabase* syntax, const QString& fileType)
    : QSyntaxHighlighter(document)
    , m_syntax(syntax)
    , m_fileType(fileType)
{
    if (m_syntax) {
        for (const QString& t : m_syntax->triggerNames())
            m_triggers.insert(t.toLower());
        for (const QString& r : m_syntax->redirectNames())
            m_triggers.insert(r.toLower());
        for (const char* group : {"const", "GetHitVar", "AssertSpecial", "StageVar", "StageInfo"}) {
            for (const QString& c : m_syntax->constNames(QString::fromLatin1(group)))
                m_consts.insert(c.toLower());
        }
    }
}

void CodeHighlighter::setColorSet(const ColorSet& colors)
{
    m_formats.clear();
    for (const QString& role : colors.roles()) {
        const ColorSet::Entry e = colors.entry(role);
        QTextCharFormat f;
        f.setForeground(e.color);
        if (e.bold)
            f.setFontWeight(QFont::Bold);
        if (e.italic)
            f.setFontItalic(true);
        m_formats.insert(role, f);
    }
    const char* const names[RoleCount] = {"comment",  "block", "keyword",       "number",     "string",
                                          "function", "const", "keyword_value", "block_type", "symbol"};
    for (int r = 0; r < RoleCount; ++r)
        m_roles[std::size_t(r)] = m_formats.value(QLatin1String(names[r]));
    rehighlight();
}

void CodeHighlighter::setFileType(const QString& fileType)
{
    if (m_fileType == fileType)
        return;
    m_fileType = fileType;
    m_sections.clear();
    m_sectionKeys.clear();
    rehighlight();
}

const CodeHighlighter::SectionInfo& CodeHighlighter::sectionInfo(const QString& header)
{
    // Most lines repeat the header of the line above: its section is looked up once.
    if (const auto known = m_sectionKeys.constFind(header); known != m_sectionKeys.cend()) {
        if (const auto it = m_sections.constFind(*known); it != m_sections.cend())
            return *it;
    }
    const QString section = m_syntax ? m_syntax->sectionFor(m_fileType, header) : QString();
    const QString key = section.isEmpty() ? QStringLiteral("\x01") + header.toLower() : section.toLower();
    m_sectionKeys.insert(header, key);
    auto it = m_sections.find(key);
    if (it != m_sections.end())
        return *it;

    SectionInfo info;
    if (m_syntax && !section.isEmpty()) {
        if (const SyntaxFileType* ft = m_syntax->fileType(m_fileType)) {
            info.typed = m_syntax->isTypedSection(m_fileType, header);
            for (int index : ft->sectionBlocks.value(section.toLower())) {
                for (const SyntaxParam& p : m_syntax->params(m_fileType, ft->blocks.at(index))) {
                    if (p.name.contains(QLatin1Char('*')))
                        info.wildcardParams.insert(p.name);
                    else
                        info.params.insert(p.name.toLower());
                    for (const QString& k : p.list)
                        info.keywords.insert(k.toLower());
                }
            }
            if (info.typed)
                info.params.insert(QStringLiteral("type"));
        }
    }
    return *m_sections.insert(key, info);
}

bool CodeHighlighter::isKnownParam(const SectionInfo& info, const QString& key) const
{
    const QString k = key.toLower();
    if (info.params.contains(k))
        return true;
    for (const QString& pattern : info.wildcardParams) {
        if (SyntaxDatabase::wildcardMatch(pattern, key))
            return true;
    }
    return false;
}

void CodeHighlighter::highlightBlock(const QString& text)
{
    const CodeLine line = CodeLine::parse(text);

    QString header;
    if (line.kind == CodeLine::Kind::Section) {
        header = line.header;
    } else if (const QTextBlock prev = currentBlock().previous(); prev.isValid()) {
        if (auto* data = CodeBlockData::get(prev))
            header = data->header;
    }
    CodeBlockData* data = CodeBlockData::get(currentBlock());
    if (!data) {
        data = new CodeBlockData;
        setCurrentBlockUserData(data);
    }
    data->header = header;
    data->isHeader = line.kind == CodeLine::Kind::Section;
    if (!data->isHeader)
        data->folded = false;
    setCurrentBlockState(int(qHash(header.toLower()) & 0x7fffffff));

    if (line.commentStart >= 0)
        setFormat(line.commentStart, int(text.size()) - line.commentStart, m_roles[Comment]);

    if (line.kind == CodeLine::Kind::Section) {
        const int start = int(text.indexOf(QLatin1Char('[')));
        const int end =
            line.headerClosed ? int(text.indexOf(QLatin1Char(']'), start)) + 1 : line.headerStart + line.headerLength;
        setFormat(start, end - start, m_roles[Block]);
        return;
    }
    if (line.kind != CodeLine::Kind::KeyValue && line.kind != CodeLine::Kind::Raw)
        return;

    const SectionInfo& info = sectionInfo(header);
    bool typeLine = false;
    if (line.kind == CodeLine::Kind::KeyValue) {
        if (isKnownParam(info, line.key))
            setFormat(line.keyStart, line.keyLength, m_roles[Keyword]);
        typeLine = info.typed && line.key.compare(QLatin1String("type"), Qt::CaseInsensitive) == 0;
    }

    const QTextCharFormat& number = m_roles[Number];
    const QTextCharFormat& string = m_roles[String];
    const QTextCharFormat& function = m_roles[Function];
    const QTextCharFormat& constant = m_roles[Constant];
    const QTextCharFormat& keywordValue = m_roles[KeywordValue];
    const QTextCharFormat& blockType = m_roles[BlockType];
    const QTextCharFormat& symbol = m_roles[Symbol];

    for (qsizetype i = 0; i < line.tokens.size(); ++i) {
        const CodeToken& t = line.tokens[i];
        switch (t.kind) {
        case CodeToken::Kind::Number:
            setFormat(t.start, t.length, number);
            break;
        case CodeToken::Kind::String:
            setFormat(t.start, t.length, string);
            break;
        case CodeToken::Kind::Identifier: {
            const QString word = text.mid(t.start, t.length).toLower();
            if (typeLine) {
                setFormat(t.start, t.length, blockType);
                break;
            }
            if (i >= 2 && line.tokens[i - 1].kind == CodeToken::Kind::OpenParen &&
                kConstTriggers.contains(text.mid(line.tokens[i - 2].start, line.tokens[i - 2].length).toLower()) &&
                m_consts.contains(word)) {
                setFormat(t.start, t.length, constant);
            } else if (m_triggers.contains(word)) {
                setFormat(t.start, t.length, function);
            } else if (info.keywords.contains(word)) {
                setFormat(t.start, t.length, keywordValue);
            } else if (m_consts.contains(word)) {
                setFormat(t.start, t.length, constant);
            }
            break;
        }
        case CodeToken::Kind::Operator:
        case CodeToken::Kind::Comma:
        case CodeToken::Kind::OpenParen:
        case CodeToken::Kind::CloseParen:
        case CodeToken::Kind::OpenBracket:
        case CodeToken::Kind::CloseBracket:
            if (symbol.foreground().style() != Qt::NoBrush)
                setFormat(t.start, t.length, symbol);
            break;
        case CodeToken::Kind::Other:
            break;
        }
    }
}

} // namespace OFS
