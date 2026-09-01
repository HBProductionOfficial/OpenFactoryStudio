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

    bool isUntitled() const { return m_untitled; }
    // A never-saved Project > New document. Its tab has no caption.
    bool isNameless() const { return m_nameless; }
    void setNameless(bool nameless) { m_nameless = nameless; }
    void setPlaceholderPath(const QString& path);
    bool isCopy() const { return m_copy; }
    void markAsCopy();
    void markUntitled() { m_untitled = true; }

    virtual bool load(QString* error) = 0;
    virtual bool save(QString* error);
    virtual bool saveAs(const QString& path, QString* error) = 0;
    virtual bool isModified() const = 0;
    virtual bool writeCopy(const QString& path, QString* error) const;

Q_SIGNALS:
    void modifiedChanged(bool modified);
    void saved();
    void pathChanged(const QString& oldPath);

protected:
    void setPath(const QString& path);
    void setUntitled(bool untitled) { m_untitled = untitled; }

private:
    QString m_path;
    QString m_editorId;
    bool m_untitled = false;
    bool m_nameless = false;
    bool m_copy = false;
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
    void createEmpty(const QString& text = QString());
    bool saveAs(const QString& path, QString* error) override;
    bool isModified() const override;
    bool writeCopy(const QString& path, QString* error) const override;

    QString encoding() const { return m_encoding; }
    bool reloadWithEncoding(const QString& encoding, QString* error);
    bool setEncoding(const QString& encoding, QString* error);
    bool saveAsUtf8(QString* error);

    // Tries byte order marks, then UTF-8 when the bytes are valid UTF-8, then the encoding
    // detector, else the fallback encoding. Sets *encoding and *bom.
    static QString decode(const QByteArray& data, QString* encoding, QByteArray* bom);
    static QByteArray encode(const QString& text, const QString& encoding);
    static QString detectEncoding(const QByteArray& data);

    // For files that are neither Unicode nor valid UTF-8. "System" by default.
    static QString fallbackEncoding();
    static void setFallbackEncoding(const QString& encoding);

    static void setTabConversion(bool tabsToSpacesOnOpen, bool spacesToTabsOnSave, int tabWidth = 4);
    static QString expandTabs(const QString& text, int tabWidth);
    static QString tabifyIndentation(const QString& text, int tabWidth);

private:
    QTextDocument* m_document;
    QByteArray m_original;
    QString m_encoding;
    QByteArray m_bom;
    QString m_lineEnding = QStringLiteral("\r\n");
};

} // namespace OFS
