#include "core/project/ProjectFile.h"

#include "core/text/Encodings.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPlainTextDocumentLayout>
#include <QSaveFile>
#include <QStringConverter>
#include <QTextDocument>

#include <uchardet.h>

namespace OFS {

namespace {
QString s_fallbackEncoding = QStringLiteral("System");
bool s_tabsToSpaces = false;
bool s_spacesToTabs = false;
int s_tabWidth = 4;
} // namespace

ProjectFile::ProjectFile(const QString& path, const QString& editorId, QObject* parent)
    : QObject(parent)
    , m_path(QDir::fromNativeSeparators(path))
    , m_editorId(editorId)
{}

ProjectFile::~ProjectFile() = default;

QString ProjectFile::fileName() const
{
    return QFileInfo(m_path).fileName();
}

CodeFile::CodeFile(const QString& path, const QString& editorId, QObject* parent)
    : ProjectFile(path, editorId, parent)
    , m_document(new QTextDocument(this))
{
    m_document->setDocumentLayout(new QPlainTextDocumentLayout(m_document));
    connect(m_document, &QTextDocument::modificationChanged, this, &ProjectFile::modifiedChanged);
}

CodeFile::~CodeFile()
{
    // The document must not report changes while this object is half destroyed.
    disconnect(m_document, nullptr, this, nullptr);
    delete m_document;
}

QString CodeFile::decode(const QByteArray& data, QString* encoding, QByteArray* bom)
{
    bom->clear();
    if (data.startsWith("\xEF\xBB\xBF")) {
        *encoding = QStringLiteral("UTF-8");
        *bom = data.left(3);
        return QString::fromUtf8(data.mid(3));
    }
    static const QByteArray utf32le("\xFF\xFE\x00\x00", 4);
    static const QByteArray utf32be("\x00\x00\xFE\xFF", 4);
    if (data.startsWith(utf32le) || data.startsWith(utf32be)) {
        const bool le = data.startsWith(utf32le);
        *encoding = le ? QStringLiteral("UTF-32LE") : QStringLiteral("UTF-32BE");
        *bom = data.left(4);
        QStringDecoder d(le ? QStringConverter::Utf32LE : QStringConverter::Utf32BE);
        return d(data.mid(4));
    }
    if (data.startsWith("\xFF\xFE") || data.startsWith("\xFE\xFF")) {
        const bool le = data.startsWith("\xFF\xFE");
        *encoding = le ? QStringLiteral("UTF-16LE") : QStringLiteral("UTF-16BE");
        *bom = data.left(2);
        QStringDecoder d(le ? QStringConverter::Utf16LE : QStringConverter::Utf16BE);
        return d(data.mid(2));
    }
    QStringDecoder utf8(QStringConverter::Utf8);
    const QString text = utf8(data);
    bool ascii = true;
    for (char c : data) {
        if (static_cast<unsigned char>(c) >= 0x80) {
            ascii = false;
            break;
        }
    }
    if (!utf8.hasError() && !ascii) {
        *encoding = QStringLiteral("UTF-8");
        return text;
    }
    if (ascii) {
        // Plain ASCII decodes the same everywhere. Fighter Factory shows such files as UTF-8.
        *encoding = QStringLiteral("UTF-8");
        return text;
    }
    const QString detected = detectEncoding(data);
    if (!detected.isEmpty()) {
        *encoding = detected;
        return TextEncoding::decode(data, detected);
    }
    bool ok = false;
    const QString decoded = TextEncoding::decode(data, s_fallbackEncoding, &ok);
    if (!ok) {
        *encoding = QStringLiteral("System");
        return QString::fromLocal8Bit(data);
    }
    *encoding = s_fallbackEncoding;
    return decoded;
}

QByteArray CodeFile::encode(const QString& text, const QString& encoding)
{
    bool ok = false;
    const QByteArray bytes = TextEncoding::encode(text, encoding, &ok);
    return ok ? bytes : text.toUtf8();
}

QString CodeFile::detectEncoding(const QByteArray& data)
{
    uchardet_t detector = uchardet_new();
    if (!detector)
        return {};
    QString name;
    if (uchardet_handle_data(detector, data.constData(), size_t(data.size())) == 0) {
        uchardet_data_end(detector);
        name = QString::fromLatin1(uchardet_get_charset(detector));
    }
    uchardet_delete(detector);
    // Only encodings the Encoding menu offers. The detector also knows rarer ones.
    const int index = TextEncoding::indexOf(name);
    if (index <= 0)
        return {};
    const QString codec = TextEncoding::all().at(index).codecName();
    if (!TextEncoding::isAvailable(codec))
        return {};
    // With only a few non-ASCII bytes a single-byte guess is close to chance, because "café" fits
    // any Latin code page. Multi-byte Asian encodings are still found reliably.
    static const QStringList multiByte = {QStringLiteral("Shift_JIS"),   QStringLiteral("EUC-JP"),
                                          QStringLiteral("ISO-2022-JP"), QStringLiteral("GBK"),
                                          QStringLiteral("GB18030"),     QStringLiteral("GB2312"),
                                          QStringLiteral("Big5"),        QStringLiteral("Big5-HKSCS"),
                                          QStringLiteral("EUC-KR"),      QStringLiteral("windows-949")};
    qsizetype high = 0;
    for (char c : data) {
        if (static_cast<unsigned char>(c) >= 0x80)
            ++high;
    }
    if (high < 16 && !multiByte.contains(codec, Qt::CaseInsensitive))
        return {};
    return codec;
}

QString CodeFile::fallbackEncoding()
{
    return s_fallbackEncoding;
}

void CodeFile::setTabConversion(bool tabsToSpacesOnOpen, bool spacesToTabsOnSave, int tabWidth)
{
    s_tabsToSpaces = tabsToSpacesOnOpen;
    s_spacesToTabs = spacesToTabsOnSave;
    s_tabWidth = qMax(1, tabWidth);
}

QString CodeFile::expandTabs(const QString& text, int tabWidth)
{
    if (!text.contains(QLatin1Char('\t')))
        return text;
    QString result;
    result.reserve(text.size() + text.size() / 4);
    int column = 0;
    for (QChar c : text) {
        if (c == QLatin1Char('\t')) {
            const int spaces = tabWidth - column % tabWidth;
            result += QString(spaces, QLatin1Char(' '));
            column += spaces;
        } else {
            result += c;
            column = c == QLatin1Char('\n') ? 0 : column + 1;
        }
    }
    return result;
}

QString CodeFile::tabifyIndentation(const QString& text, int tabWidth)
{
    QStringList lines = text.split(QLatin1Char('\n'));
    for (QString& line : lines) {
        int width = 0;
        int end = 0;
        while (end < line.size() && (line.at(end) == QLatin1Char(' ') || line.at(end) == QLatin1Char('\t'))) {
            width = line.at(end) == QLatin1Char('\t') ? (width / tabWidth + 1) * tabWidth : width + 1;
            ++end;
        }
        if (end == 0 || width < tabWidth)
            continue;
        line =
            QString(width / tabWidth, QLatin1Char('\t')) + QString(width % tabWidth, QLatin1Char(' ')) + line.mid(end);
    }
    return lines.join(QLatin1Char('\n'));
}

void CodeFile::setFallbackEncoding(const QString& encoding)
{
    s_fallbackEncoding = TextEncoding::isAvailable(encoding) ? encoding : QStringLiteral("System");
}

bool CodeFile::load(QString* error)
{
    QFile file(path());
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    m_original = file.readAll();
    QString text = decode(m_original, &m_encoding, &m_bom);
    m_lineEnding = text.contains(QLatin1String("\r\n")) ? QStringLiteral("\r\n")
                   : text.contains(QLatin1Char('\n'))   ? QStringLiteral("\n")
                                                        : QStringLiteral("\r\n");
    text.replace(QLatin1String("\r\n"), QLatin1String("\n"));
    if (s_tabsToSpaces)
        text = expandTabs(text, s_tabWidth);
    m_document->setPlainText(text);
    m_document->setModified(false);
    return true;
}

bool CodeFile::isModified() const
{
    return m_document->isModified();
}

bool CodeFile::saveAs(const QString& target, QString* error)
{
    QByteArray bytes;
    if (!isModified() && target == path()) {
        bytes = m_original; // unchanged: write back exactly what was read
    } else {
        QString text = m_document->toPlainText();
        if (s_spacesToTabs)
            text = tabifyIndentation(text, s_tabWidth);
        if (m_lineEnding != QLatin1String("\n"))
            text.replace(QLatin1Char('\n'), m_lineEnding);
        bytes = m_bom + encode(text, m_encoding);
    }
    QDir().mkpath(QFileInfo(target).absolutePath());
    QSaveFile file(target);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    m_original = bytes;
    setPath(QDir::fromNativeSeparators(target));
    m_document->setModified(false);
    Q_EMIT saved();
    return true;
}

bool CodeFile::reloadWithEncoding(const QString& encoding, QString* error)
{
    // The byte order mark of the saved bytes is kept only when reading them as Unicode again.
    QString detected;
    QByteArray bom;
    decode(m_original, &detected, &bom);
    if (!encoding.startsWith(QLatin1String("UTF-"), Qt::CaseInsensitive))
        bom.clear();
    bool ok = false;
    QString text = TextEncoding::decode(m_original.mid(bom.size()), encoding, &ok);
    if (!ok) {
        if (error)
            *error = tr("The encoding '%1' is not supported.").arg(encoding);
        return false;
    }
    m_encoding = encoding;
    m_bom = bom;
    text.replace(QLatin1String("\r\n"), QLatin1String("\n"));
    m_document->setPlainText(text);
    m_document->setModified(false);
    return true;
}

bool CodeFile::setEncoding(const QString& encoding, QString* error)
{
    if (!TextEncoding::isAvailable(encoding)) {
        if (error)
            *error = tr("The encoding '%1' is not supported.").arg(encoding);
        return false;
    }
    if (encoding == m_encoding)
        return true;
    m_encoding = encoding;
    // A byte order mark belongs to the Unicode encoding it was read with.
    m_bom.clear();
    m_document->setModified(true);
    return true;
}

bool CodeFile::saveAsUtf8(QString* error)
{
    m_encoding = QStringLiteral("UTF-8");
    m_document->setModified(true);
    return save(error);
}

} // namespace OFS
