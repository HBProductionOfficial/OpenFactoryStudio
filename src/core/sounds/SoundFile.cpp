#include "core/sounds/SoundFile.h"

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
    m_undo->setUndoLimit(undoLimit());
    connect(m_undo, &QUndoStack::cleanChanged, this, [this](bool clean) { Q_EMIT modifiedChanged(!clean); });
}

SoundFile::~SoundFile() = default;

bool SoundFile::load(QString* error)
{
    // Read in a background task, then taken in here.
    QByteArray data;
    SoundDocument document;
    QString problem;
    const QString file = path();
    auto read = [&] {
        QFile in(file);
        if (!in.open(QIODevice::ReadOnly)) {
            problem = in.errorString();
            return;
        }
        data = in.readAll();
        auto snd = Formats::SndDocument::read(
            Formats::ByteSpan(reinterpret_cast<const std::byte*>(data.constData()), std::size_t(data.size())));
        if (!snd) {
            problem = QString::fromStdString(snd.error().message);
            return;
        }
        document = SoundDocument::fromSnd(snd.value());
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

void SoundFile::saveAsync(std::function<void(bool ok, const QString& error)> done)
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
    const SoundDocument snapshot = m_document;
    const QByteArray original = !isModified() ? m_original : QByteArray();
    const QString target = path();
    const int index = m_undo->index();
    AppContext* context = AppContext::instance();
    const int task = context ? context->tasks().begin(tr("Saving %1").arg(fileName())) : 0;
    setSaving(true);
    auto* watcher = new QFutureWatcher<Result>(this);
    connect(watcher, &QFutureWatcher<Result>::finished, this, [this, watcher, done, index, task] {
        const Result result = watcher->result();
        watcher->deleteLater();
        setSaving(false);
        if (AppContext* ctx = AppContext::instance())
            ctx->tasks().end(task);
        if (result.ok) {
            m_original = result.bytes;
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
            const Formats::Bytes bytes = snapshot.toSnd().write();
            result.bytes = QByteArray(reinterpret_cast<const char*>(bytes.data()), qsizetype(bytes.size()));
        }
        if (!writeFileSafely(target, result.bytes, &result.error))
            return result;
        result.ok = true;
        return result;
    }));
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
    if (!writeFileSafely(target, bytes, error))
        return false;
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
