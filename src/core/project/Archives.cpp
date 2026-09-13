#include "core/project/Archives.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QLibrary>
#include <QPluginLoader>

#include <algorithm>

namespace OFS {

namespace {

struct ArchiveTexts
{
    Q_DECLARE_TR_FUNCTIONS(OFS::ArchiveTexts)
};

class ZipFormat final : public ArchiveFormat
{
public:
    QString name() const override { return ArchiveTexts::tr("ZIP Archive"); }
    QStringList extensions() const override { return {QStringLiteral("zip")}; }
    QList<Zip::Entry> entries(const QString& archive, QString* error) const override
    {
        return Zip::entries(archive, error);
    }
    bool extract(const QString& archive, const QString& folder, QString* error) const override
    {
        return Zip::extract(archive, folder, error);
    }
    QByteArray read(const QString& archive, const QString& name, QString* error) const override
    {
        return Zip::read(archive, name, error);
    }
};

std::vector<std::unique_ptr<ArchiveFormat>>& registry()
{
    static std::vector<std::unique_ptr<ArchiveFormat>> formats = [] {
        std::vector<std::unique_ptr<ArchiveFormat>> list;
        list.push_back(std::make_unique<ZipFormat>());
        return list;
    }();
    return formats;
}

const ArchiveFormat* require(const QString& archive, QString* error)
{
    const ArchiveFormat* format = Archives::formatOf(archive);
    if (!format && error)
        *error = ArchiveTexts::tr("%1 is not an archive that can be read.").arg(QDir::toNativeSeparators(archive));
    return format;
}

} // namespace

namespace Archives {

void add(std::unique_ptr<ArchiveFormat> format)
{
    if (format)
        registry().push_back(std::move(format));
}

QStringList loadExtensions(const QStringList& searchDirs)
{
    QStringList errors;
    for (const QString& root : searchDirs) {
        const QDir rootDir(root);
        if (!rootDir.exists())
            continue;
        for (const QString& sub : rootDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
            const QDir dir(rootDir.filePath(sub));
            for (const QString& file : dir.entryList(QDir::Files, QDir::Name)) {
                const QString path = dir.filePath(file);
                if (!QLibrary::isLibrary(path))
                    continue;
                QPluginLoader loader(path);
                if (loader.metaData().value(QStringLiteral("IID")).toString() !=
                    QLatin1String(OFS_EXTENSION_PLUGIN_IID))
                    continue; // a helper library shipped next to the plugin
                auto* plugin = qobject_cast<ExtensionPlugin*>(loader.instance());
                if (!plugin) {
                    errors << QStringLiteral("%1: %2").arg(path, loader.errorString());
                    continue;
                }
                for (std::unique_ptr<ArchiveFormat>& format : plugin->archiveFormats())
                    add(std::move(format));
            }
        }
    }
    return errors;
}

const ArchiveFormat* formatOf(const QString& path)
{
    const QString suffix = QFileInfo(path).suffix();
    for (const std::unique_ptr<ArchiveFormat>& format : registry()) {
        if (format->extensions().contains(suffix, Qt::CaseInsensitive))
            return format.get();
    }
    return nullptr;
}

QString openFilter()
{
    struct Item
    {
        QString extension;
        QString filter;
    };
    QList<Item> items;
    for (const std::unique_ptr<ArchiveFormat>& format : registry()) {
        QStringList patterns;
        for (const QString& extension : format->extensions())
            patterns << QStringLiteral("*.") + extension;
        items.append(
            {format->extensions().value(0), QStringLiteral("%1 (%2)").arg(format->name(), patterns.join(' '))});
    }
    std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.extension < b.extension; });
    QStringList all;
    QStringList filters;
    for (const Item& item : std::as_const(items)) {
        all << item.filter.section(QLatin1Char('('), 1).chopped(1);
        filters << item.filter;
    }
    filters.prepend(ArchiveTexts::tr("All supported (%1)").arg(all.join(QLatin1Char(' '))));
    return filters.join(QStringLiteral(";;"));
}

QList<Zip::Entry> entries(const QString& archive, QString* error)
{
    const ArchiveFormat* format = require(archive, error);
    return format ? format->entries(archive, error) : QList<Zip::Entry>();
}

bool extract(const QString& archive, const QString& folder, QString* error)
{
    const ArchiveFormat* format = require(archive, error);
    return format && format->extract(archive, folder, error);
}

QByteArray read(const QString& archive, const QString& name, QString* error)
{
    const ArchiveFormat* format = require(archive, error);
    return format ? format->read(archive, name, error) : QByteArray();
}

} // namespace Archives

} // namespace OFS
