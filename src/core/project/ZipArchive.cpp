#include "core/project/ZipArchive.h"

#include "core/project/ContentPaths.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QSaveFile>

#include <miniz.h>

#include <algorithm>
#include <limits>

namespace OFS::Zip {

namespace {

struct ZipTexts
{
    Q_DECLARE_TR_FUNCTIONS(OFS::Zip::ZipTexts)
};

void fail(QString* error, const QString& message)
{
    if (error)
        *error = message;
}

size_t readAt(void* opaque, mz_uint64 offset, void* buffer, size_t n)
{
    auto* file = static_cast<QFile*>(opaque);
    if (!file->seek(qint64(offset)))
        return 0;
    const qint64 got = file->read(static_cast<char*>(buffer), qint64(n));
    return got < 0 ? 0 : size_t(got);
}

// Reads through QFile, so paths can hold any character and the archive isn't loaded into memory.
struct Reader
{
    QFile file;
    mz_zip_archive zip{};
    bool open = false;

    bool load(const QString& path, QString* error)
    {
        file.setFileName(path);
        if (!file.open(QIODevice::ReadOnly)) {
            fail(error, file.errorString());
            return false;
        }
        zip.m_pRead = readAt;
        zip.m_pIO_opaque = &file;
        open = mz_zip_reader_init(&zip, mz_uint64(file.size()), 0);
        if (!open)
            fail(error, ZipTexts::tr("%1 is not a ZIP archive.").arg(QDir::toNativeSeparators(path)));
        return open;
    }
    ~Reader()
    {
        if (open)
            mz_zip_reader_end(&zip);
    }
};

// Where unpacked bytes go, and how many more may come.
struct Sink
{
    QIODevice* out = nullptr;
    qint64 left = 0;
    bool tooBig = false;
};

size_t writeChunk(void* opaque, mz_uint64, const void* data, size_t n)
{
    auto* sink = static_cast<Sink*>(opaque);
    if (qint64(n) > sink->left) {
        sink->tooBig = true;
        return 0;
    }
    sink->left -= qint64(n);
    return sink->out->write(static_cast<const char*>(data), qint64(n)) == qint64(n) ? n : 0;
}

QString tooBig(const QString& archive, qint64 limit)
{
    return ZipTexts::tr("%1 unpacks to more than %2, so it wasn't unpacked.")
        .arg(QFileInfo(archive).fileName(), QLocale().formattedDataSize(limit));
}

QString entryName(mz_zip_archive* zip, mz_uint index)
{
    mz_zip_archive_file_stat stat{};
    if (!mz_zip_reader_file_stat(zip, index, &stat))
        return {};
    // General purpose bit 11: the name is UTF-8.
    const bool utf8 = (stat.m_bit_flag & (1u << 11)) != 0;
    QString name = utf8 ? QString::fromUtf8(stat.m_filename) : QString::fromLocal8Bit(stat.m_filename);
    // Some archivers (Windows PowerShell's Compress-Archive) separate folders with backslashes.
    return name.replace(QLatin1Char('\\'), QLatin1Char('/'));
}

} // namespace

qint64 extractLimit(qint64 archiveSize)
{
    // Characters and stages pack to a third of their size or so, while a small archive of zeros
    // can unpack to gigabytes.
    constexpr qint64 kAlways = qint64{2} << 30;
    constexpr qint64 kRatio = 50;
    return std::max(kAlways, std::min(archiveSize, std::numeric_limits<qint64>::max() / kRatio) * kRatio);
}

QList<Entry> entries(const QString& archive, QString* error)
{
    QList<Entry> list;
    Reader reader;
    if (!reader.load(archive, error))
        return list;
    const mz_uint count = mz_zip_reader_get_num_files(&reader.zip);
    for (mz_uint i = 0; i < count; ++i) {
        mz_zip_archive_file_stat stat{};
        if (!mz_zip_reader_file_stat(&reader.zip, i, &stat))
            continue;
        Entry entry;
        entry.name = entryName(&reader.zip, i);
        entry.size = qint64(stat.m_uncomp_size);
        entry.directory = mz_zip_reader_is_file_a_directory(&reader.zip, i) || entry.name.endsWith(QLatin1Char('/'));
        list << entry;
    }
    return list;
}

bool extract(const QString& archive, const QString& folder, QString* error)
{
    Reader reader;
    if (!reader.load(archive, error))
        return false;
    const QDir root(folder);
    const QString rootPath = QDir::cleanPath(root.absolutePath());
    const mz_uint count = mz_zip_reader_get_num_files(&reader.zip);
    const qint64 limit = extractLimit(reader.file.size());
    // The sizes the archive gives can be wrong, so the bytes are counted while unpacking too.
    quint64 stated = 0;
    for (mz_uint i = 0; i < count; ++i) {
        mz_zip_archive_file_stat stat{};
        if (mz_zip_reader_file_stat(&reader.zip, i, &stat))
            stated += stat.m_uncomp_size;
    }
    if (stated > quint64(limit)) {
        fail(error, tooBig(archive, limit));
        return false;
    }
    qint64 left = limit;
    for (mz_uint i = 0; i < count; ++i) {
        const QString name = entryName(&reader.zip, i);
        const QString target = QDir::cleanPath(root.absoluteFilePath(name));
        if (namesDriveOrStream(name) || !isInsideFolder(target, rootPath)) {
            fail(error, ZipTexts::tr("The archive entry %1 points outside the folder.").arg(name));
            return false;
        }
        if (mz_zip_reader_is_file_a_directory(&reader.zip, i) || name.endsWith(QLatin1Char('/'))) {
            QDir().mkpath(target);
            continue;
        }
        QDir().mkpath(QFileInfo(target).absolutePath());
        QSaveFile out(target);
        if (!out.open(QIODevice::WriteOnly)) {
            fail(error, out.errorString());
            return false;
        }
        Sink sink{&out, left};
        const bool unpacked = mz_zip_reader_extract_to_callback(&reader.zip, i, writeChunk, &sink, 0);
        if (sink.tooBig) {
            fail(error, tooBig(archive, limit));
            return false;
        }
        if (!unpacked) {
            fail(error, ZipTexts::tr("Cannot extract %1.").arg(name));
            return false;
        }
        if (!out.commit()) {
            fail(error, out.errorString());
            return false;
        }
        left = sink.left;
    }
    return true;
}

QByteArray read(const QString& archive, const QString& name, QString* error)
{
    Reader reader;
    if (!reader.load(archive, error))
        return {};
    // This one stays in memory.
    const qint64 limit = std::min(extractLimit(reader.file.size()), qint64{1} << 30);
    const mz_uint count = mz_zip_reader_get_num_files(&reader.zip);
    for (mz_uint i = 0; i < count; ++i) {
        if (entryName(&reader.zip, i) != name)
            continue;
        QByteArray bytes;
        QBuffer buffer(&bytes);
        buffer.open(QIODevice::WriteOnly);
        Sink sink{&buffer, limit};
        if (!mz_zip_reader_extract_to_callback(&reader.zip, i, writeChunk, &sink, 0)) {
            fail(error, sink.tooBig ? tooBig(archive, limit) : ZipTexts::tr("Cannot extract %1.").arg(name));
            return {};
        }
        return bytes;
    }
    fail(error, ZipTexts::tr("%1 is not in the archive.").arg(name));
    return {};
}

bool create(const QString& archive, const QString& root, const QStringList& files, QString* error)
{
    mz_zip_archive zip{};
    if (!mz_zip_writer_init_heap(&zip, 0, 0)) {
        fail(error, ZipTexts::tr("Cannot create the archive."));
        return false;
    }
    const QDir base(root);
    for (const QString& relative : files) {
        QFile file(base.absoluteFilePath(relative));
        if (!file.open(QIODevice::ReadOnly)) {
            fail(error, file.errorString());
            mz_zip_writer_end(&zip);
            return false;
        }
        const QByteArray data = file.readAll();
        const QByteArray name = QDir::fromNativeSeparators(relative).toUtf8();
        if (!mz_zip_writer_add_mem(&zip, name.constData(), data.constData(), size_t(data.size()),
                                   mz_uint(MZ_DEFAULT_LEVEL))) {
            fail(error, ZipTexts::tr("Cannot add %1 to the archive.").arg(relative));
            mz_zip_writer_end(&zip);
            return false;
        }
    }
    void* buffer = nullptr;
    size_t size = 0;
    const bool finished = mz_zip_writer_finalize_heap_archive(&zip, &buffer, &size);
    QSaveFile out(archive);
    const bool written = finished && out.open(QIODevice::WriteOnly) &&
                         out.write(static_cast<const char*>(buffer), qint64(size)) == qint64(size) && out.commit();
    mz_free(buffer); // the finished archive belongs to the caller
    mz_zip_writer_end(&zip);
    if (!written)
        fail(error, finished ? out.errorString() : ZipTexts::tr("Cannot create the archive."));
    return written;
}

} // namespace OFS::Zip
