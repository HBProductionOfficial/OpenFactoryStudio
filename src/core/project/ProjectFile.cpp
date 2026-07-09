#include "core/project/ProjectFile.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPlainTextDocumentLayout>
#include <QSaveFile>
#include <QStringConverter>
#include <QTextDocument>

namespace OFS {

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
    *encoding = QStringLiteral("System");
    return QString::fromLocal8Bit(data);
}

QByteArray CodeFile::encode(const QString& text, const QString& encoding)
{
    if (encoding == QLatin1String("System"))
        return text.toLocal8Bit();
    if (auto e = QStringConverter::encodingForName(encoding.toLatin1())) {
        QStringEncoder enc(*e);
        return enc(text);
    }
    return text.toUtf8();
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
    QString text;
    if (encoding == QLatin1String("System")) {
        text = QString::fromLocal8Bit(m_original.mid(m_bom.size()));
    } else if (auto e = QStringConverter::encodingForName(encoding.toLatin1())) {
        QStringDecoder d(*e);
        text = d(m_original.mid(m_bom.size()));
    } else {
        if (error)
            *error = tr("The encoding '%1' is not supported.").arg(encoding);
        return false;
    }
    m_encoding = encoding;
    text.replace(QLatin1String("\r\n"), QLatin1String("\n"));
    m_document->setPlainText(text);
    m_document->setModified(false);
    return true;
}

bool CodeFile::saveAsUtf8(QString* error)
{
    m_encoding = QStringLiteral("UTF-8");
    m_document->setModified(true);
    return save(error);
}

} // namespace OFS
