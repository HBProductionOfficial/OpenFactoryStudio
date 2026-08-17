#include "core/sprites/SpriteFile.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QUndoCommand>
#include <QUndoStack>

namespace OFS {

class SpriteEditCommand : public QUndoCommand
{
public:
    // `axisOf` >= 0: an axis change of that sprite, merged with the next one of the same sprite.
    SpriteEditCommand(SpriteFile* file, const QString& text, SpriteDocument before, SpriteDocument after,
                      int axisOf = -1)
        : QUndoCommand(text)
        , m_file(file)
        , m_before(std::move(before))
        , m_after(std::move(after))
        , m_axisOf(axisOf)
    {}

    void undo() override { m_file->setDocument(m_before); }
    void redo() override { m_file->setDocument(m_after); }
    int id() const override { return m_axisOf >= 0 ? 1 : -1; }
    bool mergeWith(const QUndoCommand* other) override
    {
        const auto* next = static_cast<const SpriteEditCommand*>(other);
        if (next->m_axisOf != m_axisOf)
            return false;
        m_after = next->m_after;
        return true;
    }

private:
    SpriteFile* m_file;
    SpriteDocument m_before;
    SpriteDocument m_after;
    int m_axisOf;
};

SpriteFile::SpriteFile(const QString& path, const QString& editorId, QObject* parent)
    : ProjectFile(path, editorId, parent)
    , m_undo(new QUndoStack(this))
{
    connect(m_undo, &QUndoStack::cleanChanged, this, [this](bool clean) { Q_EMIT modifiedChanged(!clean); });
}

SpriteFile::~SpriteFile() = default;

bool SpriteFile::load(QString* error)
{
    QFile file(path());
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    const QByteArray data = file.readAll();
    auto sff = Formats::SffDocument::read(
        Formats::ByteSpan(reinterpret_cast<const std::byte*>(data.constData()), std::size_t(data.size())));
    if (!sff) {
        if (error)
            *error = tr("%1 is not a sprite file that can be read: %2")
                         .arg(fileName(), QString::fromStdString(sff.error().message));
        return false;
    }
    m_original = data;
    m_document = SpriteDocument::fromSff(sff.value());
    m_undo->clear();
    Q_EMIT documentChanged();
    return true;
}

QByteArray SpriteFile::bytesToWrite(Formats::SffVersion version, QString* error) const
{
    if (!isModified() && version == m_document.version() && !m_original.isEmpty())
        return m_original; // unchanged: exactly what was read
    Formats::SffDocument sff = m_document.toSff();
    if (version != sff.version) {
        const Formats::Status converted = sff.convertTo(version);
        if (!converted) {
            if (error)
                *error = QString::fromStdString(converted.error().message);
            return {};
        }
    }
    auto bytes = sff.write();
    if (!bytes) {
        if (error)
            *error = QString::fromStdString(bytes.error().message);
        return {};
    }
    return QByteArray(reinterpret_cast<const char*>(bytes.value().data()), qsizetype(bytes.value().size()));
}

bool SpriteFile::saveAs(const QString& target, QString* error)
{
    return saveAs(target, m_document.version(), error);
}

bool SpriteFile::saveAs(const QString& target, Formats::SffVersion version, QString* error)
{
    const QByteArray bytes = bytesToWrite(version, error);
    if (bytes.isEmpty())
        return false;
    QDir().mkpath(QFileInfo(target).absolutePath());
    QSaveFile file(target);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    m_original = bytes;
    if (version != m_document.version()) {
        // The document is now what was written (the conversion cannot be undone).
        auto written = Formats::SffDocument::read(
            Formats::ByteSpan(reinterpret_cast<const std::byte*>(bytes.constData()), std::size_t(bytes.size())));
        if (written) {
            m_document = SpriteDocument::fromSff(written.value());
            m_undo->clear();
            Q_EMIT documentChanged();
        }
    }
    setPath(QDir::fromNativeSeparators(target));
    m_undo->setClean();
    Q_EMIT saved();
    return true;
}

bool SpriteFile::isModified() const
{
    return !m_undo->isClean();
}

bool SpriteFile::writeCopy(const QString& target, QString* error) const
{
    const QByteArray bytes = bytesToWrite(m_document.version(), error);
    if (bytes.isEmpty())
        return false;
    QFile file(target);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
}

void SpriteFile::apply(const QString& text, const SpriteDocument& after)
{
    m_undo->push(new SpriteEditCommand(this, text, m_document, after));
}

void SpriteFile::applyAxisChange(int index, const SpriteDocument& after)
{
    m_undo->push(new SpriteEditCommand(this, tr("Sprite axis"), m_document, after, index));
}

void SpriteFile::setDocument(const SpriteDocument& document)
{
    m_document = document;
    Q_EMIT documentChanged();
}

} // namespace OFS
