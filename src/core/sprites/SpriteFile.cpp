#include "core/sprites/SpriteFile.h"

#include "core/app/AppContext.h"
#include "core/app/TaskList.h"
#include "core/project/FileWriter.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QUndoCommand>
#include <QUndoStack>
#include <QtConcurrent/QtConcurrentRun>

namespace OFS {

class SpriteEditCommand : public QUndoCommand
{
public:
    // `axisOf` >= 0: an axis change of that sprite, merged with the next one of the same sprite.
    SpriteEditCommand(SpriteFile* file, const QString& text, SpriteDocument before, SpriteDocument after,
                      int axisOf = -1, bool colorEdit = false)
        : QUndoCommand(text)
        , m_file(file)
        , m_before(std::move(before))
        , m_after(std::move(after))
        , m_axisOf(axisOf)
        , m_colorEdit(colorEdit)
    {}

    bool isColorEdit() const { return m_colorEdit; }

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
    bool m_colorEdit;
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
    QByteArray data;
    SpriteDocument document;
    QString problem;
    const QString file = path();
    auto read = [&] {
        QFile in(file);
        if (!in.open(QIODevice::ReadOnly)) {
            problem = in.errorString();
            return;
        }
        data = in.readAll();
        auto sff = Formats::SffDocument::read(
            Formats::ByteSpan(reinterpret_cast<const std::byte*>(data.constData()), std::size_t(data.size())));
        if (!sff) {
            problem = tr("%1 is not a sprite file that can be read: %2")
                          .arg(QFileInfo(file).fileName(), QString::fromStdString(sff.error().message));
            return;
        }
        document = SpriteDocument::fromSff(sff.value());
    };
    if (AppContext* context = AppContext::instance())
        context->tasks().run(tr("Loading %1").arg(fileName()), read);
    else
        read();
    if (!problem.isEmpty()) {
        if (error)
            *error = problem;
        return false;
    }
    m_original = data;
    m_document = document;
    m_undo->clear();
    Q_EMIT documentChanged();
    return true;
}

void SpriteFile::saveAsync(std::function<void(bool ok, const QString& error)> done)
{
    if (isUntitled()) {
        ProjectFile::saveAsync(std::move(done));
        return;
    }
    struct Result
    {
        bool ok = false;
        QString error;
        QByteArray bytes;
    };
    const SpriteDocument snapshot = m_document;
    const QByteArray original = !isModified() ? m_original : QByteArray();
    const QString target = path();
    const int index = m_undo->index();
    AppContext* context = AppContext::instance();
    const int task = context ? context->tasks().begin(tr("Saving %1").arg(fileName())) : 0;
    auto* watcher = new QFutureWatcher<Result>(this);
    connect(watcher, &QFutureWatcher<Result>::finished, this, [this, watcher, done, index, task] {
        const Result result = watcher->result();
        watcher->deleteLater();
        if (AppContext* ctx = AppContext::instance())
            ctx->tasks().end(task);
        if (result.ok) {
            m_original = result.bytes;
            // Saved as it was when the save began: clean unless edited since.
            if (m_undo->index() == index)
                m_undo->setClean();
            Q_EMIT saved();
        }
        if (done)
            done(result.ok, result.error);
    });
    watcher->setFuture(QtConcurrent::run([snapshot, original, target]() {
        Result result;
        if (!original.isEmpty()) {
            result.bytes = original;
        } else {
            auto bytes = snapshot.toSff().write();
            if (!bytes) {
                result.error = QString::fromStdString(bytes.error().message);
                return result;
            }
            result.bytes =
                QByteArray(reinterpret_cast<const char*>(bytes.value().data()), qsizetype(bytes.value().size()));
        }
        if (!writeFileSafely(target, result.bytes, &result.error))
            return result;
        result.ok = true;
        return result;
    }));
}

void SpriteFile::createEmpty(Formats::SffVersion version)
{
    Formats::SffDocument sff;
    sff.version = version;
    m_original.clear();
    m_document = SpriteDocument::fromSff(sff);
    m_undo->clear();
    setUntitled(true);
    Q_EMIT documentChanged();
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
    if (!writeFileSafely(target, bytes, error))
        return false;
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
    return isCopy() || !m_undo->isClean();
}

bool SpriteFile::writeCopy(const QString& target, QString* error) const
{
    return writeCopy(target, m_document.version(), error);
}

bool SpriteFile::writeCopy(const QString& target, Formats::SffVersion version, QString* error) const
{
    const QByteArray bytes = bytesToWrite(version, error);
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

void SpriteFile::applyColorEdit(const QString& text, const SpriteDocument& after)
{
    m_undo->push(new SpriteEditCommand(this, text, m_document, after, -1, true));
}

bool SpriteFile::undoColorEdit()
{
    const auto* last = static_cast<const SpriteEditCommand*>(m_undo->command(m_undo->index() - 1));
    if (!last || !last->isColorEdit())
        return false;
    m_undo->undo();
    return true;
}

bool SpriteFile::redoColorEdit()
{
    const auto* next = static_cast<const SpriteEditCommand*>(m_undo->command(m_undo->index()));
    if (!next || !next->isColorEdit())
        return false;
    m_undo->redo();
    return true;
}

void SpriteFile::setDocument(const SpriteDocument& document)
{
    m_document = document;
    Q_EMIT documentChanged();
}

} // namespace OFS
