#include "core/script/ScriptTranslation.h"

#include <QFileInfo>
#include <QLocale>
#include <QSaveFile>
#include <QTranslator>
#include <QXmlStreamWriter>

namespace OFS::Script {

namespace {

// Reads Lua source well enough to find string literals outside comments.
class Scanner
{
public:
    explicit Scanner(const QString& code)
        : m_code(code)
    {}

    bool atEnd() const { return m_pos >= m_code.size(); }
    QChar peek(int ahead = 0) const
    {
        const int i = m_pos + ahead;
        return i < m_code.size() ? m_code.at(i) : QChar();
    }
    void skipSpaceAndComments()
    {
        while (!atEnd()) {
            if (peek().isSpace()) {
                ++m_pos;
            } else if (peek() == QLatin1Char('-') && peek(1) == QLatin1Char('-')) {
                m_pos += 2;
                int level = -1;
                if (peek() == QLatin1Char('[') && (level = bracketLevel()) >= 0)
                    readLong(level);
                else
                    while (!atEnd() && peek() != QLatin1Char('\n'))
                        ++m_pos;
            } else {
                return;
            }
        }
    }
    int bracketLevel() const
    {
        int i = 1;
        while (peek(i) == QLatin1Char('='))
            ++i;
        return peek(i) == QLatin1Char('[') ? i - 1 : -1;
    }
    QString readLong(int level)
    {
        m_pos += level + 2;
        if (peek() == QLatin1Char('\n'))
            ++m_pos; // a line break right after the opening bracket is not part of the text
        const QString close = QLatin1Char(']') + QString(level, QLatin1Char('=')) + QLatin1Char(']');
        const int end = int(m_code.indexOf(close, m_pos));
        const QString text = m_code.mid(m_pos, end < 0 ? -1 : end - m_pos);
        m_pos = end < 0 ? int(m_code.size()) : end + int(close.size());
        return text;
    }
    QString readQuoted()
    {
        const QChar quote = m_code.at(m_pos++);
        QString text;
        while (!atEnd() && peek() != quote && peek() != QLatin1Char('\n')) {
            QChar c = m_code.at(m_pos++);
            if (c != QLatin1Char('\\') || atEnd()) {
                text += c;
                continue;
            }
            c = m_code.at(m_pos++);
            switch (c.unicode()) {
            case 'n':
                text += QLatin1Char('\n');
                break;
            case 't':
                text += QLatin1Char('\t');
                break;
            case 'r':
                text += QLatin1Char('\r');
                break;
            case 'a':
                text += QChar(7);
                break;
            case 'x': {
                bool ok = false;
                const int value = m_code.mid(m_pos, 2).toInt(&ok, 16);
                if (ok) {
                    text += QChar(value);
                    m_pos += 2;
                }
                break;
            }
            default:
                if (c.isDigit()) {
                    int digits = 1;
                    while (digits < 3 && peek().isDigit())
                        ++digits, ++m_pos;
                    text += QChar(m_code.mid(m_pos - digits, digits).toInt());
                } else {
                    text += c;
                }
            }
        }
        ++m_pos;
        return text;
    }
    bool readString(QString* text)
    {
        if (peek() == QLatin1Char('"') || peek() == QLatin1Char('\'')) {
            *text = readQuoted();
            return true;
        }
        if (peek() == QLatin1Char('[')) {
            const int level = bracketLevel();
            if (level >= 0) {
                *text = readLong(level);
                return true;
            }
        }
        return false;
    }
    QString readName()
    {
        const int start = m_pos;
        while (!atEnd() && (peek().isLetterOrNumber() || peek() == QLatin1Char('_')))
            ++m_pos;
        return m_code.mid(start, m_pos - start);
    }
    void advance() { ++m_pos; }

private:
    const QString& m_code;
    int m_pos = 0;
};

} // namespace

QStringList translatableStrings(const QString& code)
{
    QStringList strings;
    Scanner scan(code);
    QString previous; // the last two names: "FF" then "translate"
    QString beforePrevious;
    bool dotted = false;
    while (true) {
        scan.skipSpaceAndComments();
        if (scan.atEnd())
            break;
        const QChar c = scan.peek();
        if (c.isLetter() || c == QLatin1Char('_')) {
            const QString name = scan.readName();
            beforePrevious = dotted ? previous : QString();
            previous = name;
            dotted = false;
            continue;
        }
        if (c == QLatin1Char('.') && scan.peek(1) != QLatin1Char('.')) {
            scan.advance();
            dotted = true;
            continue;
        }
        if (c == QLatin1Char('(') && previous == QLatin1String("translate") &&
            (beforePrevious == QLatin1String("FF") || beforePrevious == QLatin1String("OFS"))) {
            scan.advance();
            scan.skipSpaceAndComments();
            QString text;
            if (scan.readString(&text)) {
                scan.skipSpaceAndComments();
                // Only a literal on its own: FF.translate("a" .. b) is not one text.
                if (scan.peek() == QLatin1Char(')') && !strings.contains(text))
                    strings << text;
            }
            previous.clear();
            continue;
        }
        QString ignored;
        if (!scan.readString(&ignored))
            scan.advance();
        previous.clear();
        dotted = false;
    }
    return strings;
}

bool writeTranslationFile(const QString& path, const QString& context, const QStringList& strings, QString* error)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    QXmlStreamWriter xml(&file);
    xml.setAutoFormatting(true);
    xml.setAutoFormattingIndent(4);
    xml.writeStartDocument();
    xml.writeDTD(QStringLiteral("<!DOCTYPE TS>"));
    xml.writeStartElement(QStringLiteral("TS"));
    xml.writeAttribute(QStringLiteral("version"), QStringLiteral("2.1"));
    xml.writeStartElement(QStringLiteral("context"));
    xml.writeTextElement(QStringLiteral("name"), context);
    for (const QString& text : strings) {
        xml.writeStartElement(QStringLiteral("message"));
        xml.writeTextElement(QStringLiteral("source"), text);
        xml.writeStartElement(QStringLiteral("translation"));
        xml.writeAttribute(QStringLiteral("type"), QStringLiteral("unfinished"));
        xml.writeEndElement();
        xml.writeEndElement();
    }
    xml.writeEndElement();
    xml.writeEndElement();
    xml.writeEndDocument();
    if (!file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
}

bool loadScriptTranslation(QTranslator& translator, const QString& scriptPath)
{
    const QFileInfo info(scriptPath);
    return translator.load(QLocale(), info.completeBaseName(), QStringLiteral("_"), info.absolutePath(),
                           QStringLiteral(".qm"));
}

} // namespace OFS::Script
