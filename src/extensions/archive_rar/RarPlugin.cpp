// Reads RAR archives through libarchive.

#include "core/project/Archives.h"

#include <archive.h>
#include <archive_entry.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QObject>

namespace OFS {

namespace {

// Translation context for this file. lupdate reads the class's qualified name.
struct RarTexts
{
    Q_DECLARE_TR_FUNCTIONS(OFS::RarTexts)
};

class Reader
{
public:
    explicit Reader(const QString& path)
        : m_archive(archive_read_new())
    {
        archive_read_support_format_rar(m_archive);
        archive_read_support_format_rar5(m_archive);
#if defined(Q_OS_WIN)
        m_open = archive_read_open_filename_w(m_archive, reinterpret_cast<const wchar_t*>(path.utf16()), 1 << 16) ==
                 ARCHIVE_OK;
#else
        m_open = archive_read_open_filename(m_archive, QFile::encodeName(path).constData(), 1 << 16) == ARCHIVE_OK;
#endif
    }
    ~Reader() { archive_read_free(m_archive); }
    Reader(const Reader&) = delete;
    Reader& operator=(const Reader&) = delete;

    bool isOpen() const { return m_open; }
    bool failed() const { return m_failed; }
    QString error() const
    {
        const char* text = archive_error_string(m_archive);
        return text ? QString::fromUtf8(text) : RarTexts::tr("The archive cannot be read.");
    }

    // nullptr at the end or on an error. failed() tells them apart.
    archive_entry* next()
    {
        archive_entry* entry = nullptr;
        const int result = archive_read_next_header(m_archive, &entry);
        if (result == ARCHIVE_EOF)
            return nullptr;
        if (result < ARCHIVE_WARN) {
            m_failed = true;
            return nullptr;
        }
        return entry;
    }

    QByteArray data(qint64 size)
    {
        QByteArray bytes;
        bytes.reserve(qsizetype(std::max<qint64>(size, 0)));
        char buffer[1 << 16];
        for (;;) {
            const la_ssize_t n = archive_read_data(m_archive, buffer, sizeof buffer);
            if (n == 0)
                break;
            if (n < 0) {
                m_failed = true;
                return {};
            }
            bytes.append(buffer, qsizetype(n));
        }
        return bytes;
    }

private:
    archive* m_archive;
    bool m_open = false;
    bool m_failed = false;
};

QString nameOf(archive_entry* entry)
{
    QString name;
    if (const wchar_t* wide = archive_entry_pathname_w(entry))
        name = QString::fromWCharArray(wide);
    else if (const char* utf8 = archive_entry_pathname_utf8(entry))
        name = QString::fromUtf8(utf8);
    name.replace(QLatin1Char('\\'), QLatin1Char('/'));
    while (name.endsWith(QLatin1Char('/')))
        name.chop(1);
    return name;
}

bool isDirectory(archive_entry* entry)
{
    return archive_entry_filetype(entry) == AE_IFDIR;
}

bool fail(QString* error, const QString& text)
{
    if (error)
        *error = text;
    return false;
}

class RarFormat final : public ArchiveFormat
{
public:
    QString name() const override { return RarTexts::tr("Roshal ARchive"); }
    QStringList extensions() const override { return {QStringLiteral("rar")}; }

    QList<Zip::Entry> entries(const QString& archive, QString* error) const override
    {
        Reader reader(archive);
        if (!reader.isOpen()) {
            fail(error, reader.error());
            return {};
        }
        QList<Zip::Entry> list;
        while (archive_entry* entry = reader.next())
            list.append({nameOf(entry), qint64(archive_entry_size(entry)), isDirectory(entry)});
        if (reader.failed()) {
            fail(error, reader.error());
            return {};
        }
        return list;
    }

    bool extract(const QString& archive, const QString& folder, QString* error) const override
    {
        Reader reader(archive);
        if (!reader.isOpen())
            return fail(error, reader.error());
        const QString root = QDir::cleanPath(QDir(folder).absolutePath());
        while (archive_entry* entry = reader.next()) {
            const QString name = nameOf(entry);
            if (name.isEmpty())
                continue;
            const QString target = QDir::cleanPath(root + QLatin1Char('/') + name);
            if (!target.startsWith(root + QLatin1Char('/')))
                return fail(error, RarTexts::tr("The archive entry %1 points outside the folder.").arg(name));
            if (isDirectory(entry)) {
                if (!QDir().mkpath(target))
                    return fail(error, RarTexts::tr("Cannot extract %1.").arg(name));
                continue;
            }
            const QByteArray data = reader.data(archive_entry_size(entry));
            if (reader.failed())
                return fail(error, reader.error());
            QDir().mkpath(QFileInfo(target).absolutePath());
            QFile out(target);
            if (!out.open(QIODevice::WriteOnly) || out.write(data) != data.size())
                return fail(error, RarTexts::tr("Cannot extract %1.").arg(name));
        }
        return !reader.failed() || fail(error, reader.error());
    }

    QByteArray read(const QString& archive, const QString& name, QString* error) const override
    {
        Reader reader(archive);
        if (!reader.isOpen()) {
            fail(error, reader.error());
            return {};
        }
        while (archive_entry* entry = reader.next()) {
            if (isDirectory(entry) || nameOf(entry) != name)
                continue;
            const QByteArray data = reader.data(archive_entry_size(entry));
            if (reader.failed())
                fail(error, reader.error());
            return data;
        }
        fail(error, reader.failed() ? reader.error() : RarTexts::tr("%1 is not in the archive.").arg(name));
        return {};
    }
};

} // namespace

class RarPlugin : public QObject, public ExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID OFS_EXTENSION_PLUGIN_IID FILE "archive_rar.json")
    Q_INTERFACES(OFS::ExtensionPlugin)

public:
    std::vector<std::unique_ptr<ArchiveFormat>> archiveFormats() override
    {
        std::vector<std::unique_ptr<ArchiveFormat>> formats;
        formats.push_back(std::make_unique<RarFormat>());
        return formats;
    }
};

} // namespace OFS

#include "RarPlugin.moc"
