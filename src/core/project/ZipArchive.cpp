#include "core/project/ZipArchive.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

#include <miniz.h>

#include <memory>

namespace OFS::Zip {

namespace {

QString tr(const char* text)
{
    return QCoreApplication::translate("OFS::Zip", text);
}

void fail(QString* error, const QString& message)
{
    if (error)
        *error = message;
}

// Reads the whole file in memory, so paths can hold any character and no FILE* is needed.
struct Reader
{
    QByteArray bytes;
    mz_zip_archive zip{};
    bool open = false;

    bool load(const QString& path, QString* error)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            fail(error, file.errorString());
            return false;
        }
        bytes = file.readAll();
        open = mz_zip_reader_init_mem(&zip, bytes.constData(), size_t(bytes.size()), 0);
        if (!open)
            fail(error, tr("%1 is not a ZIP archive.").arg(QDir::toNativeSeparators(path)));
        return open;
    }
    ~Reader()
    {
        if (open)
            mz_zip_reader_end(&zip);
    }
};

QString entryName(mz_zip_archive* zip, mz_uint index)
{
    mz_zip_archive_file_stat stat{};
    if (!mz_zip_reader_file_stat(zip, index, &stat))
        return {};
    // General purpose bit 11: the name is UTF-8.
    const bool utf8 = (stat.m_bit_flag & (1u << 11)) != 0;
    return utf8 ? QString::fromUtf8(stat.m_filename) : QString::fromLocal8Bit(stat.m_filename);
}

} // namespace

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
        entry.directory = mz_zip_reader_is_file_a_directory(&reader.zip, i);
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
    const QString rootPath = QDir::cleanPath(root.absolutePath()) + QLatin1Char('/');
    const mz_uint count = mz_zip_reader_get_num_files(&reader.zip);
    for (mz_uint i = 0; i < count; ++i) {
        const QString name = entryName(&reader.zip, i);
        const QString target = QDir::cleanPath(root.absoluteFilePath(name));
        if (!QString(target + QLatin1Char('/')).startsWith(rootPath, Qt::CaseInsensitive)) {
            fail(error, tr("The archive entry %1 points outside the folder.").arg(name));
            return false;
        }
        if (mz_zip_reader_is_file_a_directory(&reader.zip, i)) {
            QDir().mkpath(target);
            continue;
        }
        QDir().mkpath(QFileInfo(target).absolutePath());
        size_t size = 0;
        std::unique_ptr<void, decltype(&mz_free)> data(mz_zip_reader_extract_to_heap(&reader.zip, i, &size, 0),
                                                       &mz_free);
        if (!data && size != 0) {
            fail(error, tr("Cannot extract %1.").arg(name));
            return false;
        }
        QSaveFile out(target);
        if (!out.open(QIODevice::WriteOnly) ||
            out.write(static_cast<const char*>(data.get()), qint64(size)) != qint64(size) || !out.commit()) {
            fail(error, out.errorString());
            return false;
        }
    }
    return true;
}

QByteArray read(const QString& archive, const QString& name, QString* error)
{
    Reader reader;
    if (!reader.load(archive, error))
        return {};
    const mz_uint count = mz_zip_reader_get_num_files(&reader.zip);
    for (mz_uint i = 0; i < count; ++i) {
        if (entryName(&reader.zip, i) != name)
            continue;
        size_t size = 0;
        std::unique_ptr<void, decltype(&mz_free)> data(mz_zip_reader_extract_to_heap(&reader.zip, i, &size, 0),
                                                       &mz_free);
        return data ? QByteArray(static_cast<const char*>(data.get()), qsizetype(size)) : QByteArray();
    }
    fail(error, tr("%1 is not in the archive.").arg(name));
    return {};
}

bool create(const QString& archive, const QString& root, const QStringList& files, QString* error)
{
    mz_zip_archive zip{};
    if (!mz_zip_writer_init_heap(&zip, 0, 0)) {
        fail(error, tr("Cannot create the archive."));
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
                                   MZ_DEFAULT_COMPRESSION)) {
            fail(error, tr("Cannot add %1 to the archive.").arg(relative));
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
        fail(error, finished ? out.errorString() : tr("Cannot create the archive."));
    return written;
}

} // namespace OFS::Zip
