#include "core/sounds/SoundFile.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QUndoCommand>
#include <QUndoStack>

namespace OFS {

// Document before and after. Copies share the sound data so they are cheap.
class SoundEditCommand : public QUndoCommand
{
public:
    SoundEditCommand(SoundFile* file, const QString& text, SoundDocument before, SoundDocument after)
        : QUndoCommand(text)
        , m_file(file)
        , m_before(std::move(before))
        , m_after(std::move(after))
    {}
    void undo() override { m_file->setDocument(m_before); }
    void redo() override { m_file->setDocument(m_after); }

private:
    SoundFile* m_file;
    SoundDocument m_before;
    SoundDocument m_after;
};

SoundFile::SoundFile(const QString& path, const QString& editorId, QObject* parent)
    : ProjectFile(path, editorId, parent)
    , m_undo(new QUndoStack(this))
{
    connect(m_undo, &QUndoStack::cleanChanged, this, [this](bool clean) { Q_EMIT modifiedChanged(!clean); });
}

SoundFile::~SoundFile() = default;

bool SoundFile::load(QString* error)
{
    QFile file(path());
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    const QByteArray data = file.readAll();
    auto snd = Formats::SndDocument::read(
        Formats::ByteSpan(reinterpret_cast<const std::byte*>(data.constData()), std::size_t(data.size())));
    if (!snd) {
        if (error)
            *error = QString::fromStdString(snd.error().message);
        return false;
    }
    m_original = data;
    m_document = SoundDocument::fromSnd(snd.value());
    m_undo->clear();
    Q_EMIT documentChanged();
    return true;
}

void SoundFile::createEmpty()
{
    m_original.clear();
    m_document = SoundDocument::fromSnd(Formats::SndDocument());
    m_undo->clear();
    setUntitled(true);
    Q_EMIT documentChanged();
}

QByteArray SoundFile::bytesToWrite() const
{
    if (!isModified() && !m_original.isEmpty())
        return m_original; // unchanged: exactly what was read
    const Formats::Bytes bytes = m_document.toSnd().write();
    return QByteArray(reinterpret_cast<const char*>(bytes.data()), qsizetype(bytes.size()));
}

bool SoundFile::saveAs(const QString& target, QString* error)
{
    const QByteArray bytes = bytesToWrite();
    QDir().mkpath(QFileInfo(target).absolutePath());
    QSaveFile file(target);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    m_original = bytes;
    setPath(QDir::fromNativeSeparators(target));
    m_undo->setClean();
    Q_EMIT saved();
    return true;
}

bool SoundFile::isModified() const
{
    return isCopy() || !m_undo->isClean();
}

bool SoundFile::writeCopy(const QString& target, QString* error) const
{
    const QByteArray bytes = bytesToWrite();
    QFile file(target);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
}

void SoundFile::apply(const QString& text, const SoundDocument& after)
{
    m_undo->push(new SoundEditCommand(this, text, m_document, after));
}

void SoundFile::setDocument(const SoundDocument& document)
{
    m_document = document;
    Q_EMIT documentChanged();
}

} // namespace OFS
