#pragma once

#include "core/CoreExport.h"
#include "core/project/ProjectFile.h"
#include "core/sprites/SpriteDocument.h"

class QUndoStack;

namespace OFS {

// Each edit replaces the document as one undo step. An unchanged file is written back byte for
// byte.
class OFS_CORE_EXPORT SpriteFile : public ProjectFile
{
    Q_OBJECT

public:
    SpriteFile(const QString& path, const QString& editorId, QObject* parent = nullptr);
    ~SpriteFile() override;

    bool load(QString* error) override;
    void createEmpty(Formats::SffVersion version);
    bool saveAs(const QString& path, QString* error) override;
    bool saveAs(const QString& path, Formats::SffVersion version, QString* error);
    bool isModified() const override;
    bool writeCopy(const QString& path, QString* error) const override;
    bool writeCopy(const QString& path, Formats::SffVersion version, QString* error) const;

    const SpriteDocument& document() const { return m_document; }
    QUndoStack* undoStack() const { return m_undo; }

    void apply(const QString& text, const SpriteDocument& after);
    // Back-to-back axis changes of one sprite (spins, 1 px moves, dragging) merge into one step.
    void applyAxisChange(int index, const SpriteDocument& after);

Q_SIGNALS:
    void documentChanged();

private:
    friend class SpriteEditCommand;
    void setDocument(const SpriteDocument& document);
    QByteArray bytesToWrite(Formats::SffVersion version, QString* error) const;

    SpriteDocument m_document;
    QByteArray m_original;
    QUndoStack* m_undo;
};

} // namespace OFS
