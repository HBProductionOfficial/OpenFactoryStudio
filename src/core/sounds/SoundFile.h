#pragma once

#include "core/CoreExport.h"
#include "core/project/ProjectFile.h"
#include "core/sounds/SoundDocument.h"

class QUndoStack;

namespace OFS {

// A project's SND file. An unchanged file is written back byte for byte.
class OFS_CORE_EXPORT SoundFile : public ProjectFile
{
    Q_OBJECT

public:
    SoundFile(const QString& path, const QString& editorId, QObject* parent = nullptr);
    ~SoundFile() override;

    bool load(QString* error) override;
    // Editor > New: an untitled empty list.
    void createEmpty();
    bool saveAs(const QString& path, QString* error) override;
    bool canSaveAsync() const override { return !isUntitled(); }
    void saveAsync(std::function<void(bool ok, const QString& error)> done) override;
    bool isModified() const override;
    bool writeCopy(const QString& path, QString* error) const override;

    const SoundDocument& document() const { return m_document; }
    QUndoStack* undoStack() const { return m_undo; }

    void apply(const QString& text, const SoundDocument& after);

Q_SIGNALS:
    void documentChanged();

private:
    friend class SoundEditCommand;
    void setDocument(const SoundDocument& document);
    QByteArray bytesToWrite() const;

    SoundDocument m_document;
    QByteArray m_original; // the bytes last read or written
    QUndoStack* m_undo;
};

} // namespace OFS
