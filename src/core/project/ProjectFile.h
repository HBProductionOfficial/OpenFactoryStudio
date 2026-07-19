#pragma once

#include "core/CoreExport.h"

#include <QByteArray>
#include <QObject>
#include <QString>

class QTextDocument;

namespace OFS {

class OFS_CORE_EXPORT ProjectFile : public QObject
{
    Q_OBJECT

public:
    ProjectFile(const QString& path, const QString& editorId, QObject* parent = nullptr);
    ~ProjectFile() override;

    QString path() const { return m_path; }
    QString fileName() const;
    QString editorId() const { return m_editorId; }

    virtual bool load(QString* error) = 0;
    virtual bool save(QString* error) { return saveAs(m_path, error); }
    virtual bool saveAs(const QString& path, QString* error) = 0;
    virtual bool isModified() const = 0;

Q_SIGNALS:
    void modifiedChanged(bool modified);
    void saved();

protected:
    void setPath(const QString& path) { m_path = path; }

private:
    QString m_path;
    QString m_editorId;
};

// A text file (definitions, commands, states, animations...). The text lives in a QTextDocument
// shared by every view. The original encoding, BOM and line endings are kept, so saving an
// unchanged file writes the same bytes.
class OFS_CORE_EXPORT CodeFile : public ProjectFile
{
    Q_OBJECT

public:
    CodeFile(const QString& path, const QString& editorId, QObject* parent = nullptr);
    ~CodeFile() override;

    QTextDocument* document() const { return m_document; }

    bool load(QString* error) override;
    bool saveAs(const QString& path, QString* error) override;
    bool isModified() const override;

    QString encoding() const { return m_encoding; }
    bool reloadWithEncoding(const QString& encoding, QString* error);
    bool setEncoding(const QString& encoding, QString* error);
    bool saveAsUtf8(QString* error);

    // Tries byte order marks, then UTF-8 when the bytes are valid UTF-8, then the encoding
    // detector, else the fallback encoding. Sets *encoding and *bom.
    static QString decode(const QByteArray& data, QString* encoding, QByteArray* bom);
    static QByteArray encode(const QString& text, const QString& encoding);

    // For files that are neither Unicode nor valid UTF-8. "System" by default.
    static QString fallbackEncoding();
    static void setFallbackEncoding(const QString& encoding);

private:
    QTextDocument* m_document;
    QByteArray m_original;
    QString m_encoding;
    QByteArray m_bom;
    QString m_lineEnding = QStringLiteral("\r\n");
};

} // namespace OFS
