#pragma once

#include "core/CoreExport.h"
#include "core/project/ZipArchive.h"

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

#include <memory>
#include <vector>

namespace OFS {

// An archive format projects can be opened from. ZIP is built in and extension plugins add
// others like RAR.
class OFS_CORE_EXPORT ArchiveFormat
{
public:
    virtual ~ArchiveFormat() = default;

    virtual QString name() const = 0;
    virtual QStringList extensions() const = 0;

    virtual QList<Zip::Entry> entries(const QString& archive, QString* error) const = 0;
    // Refuses entries that would land outside folder.
    virtual bool extract(const QString& archive, const QString& folder, QString* error) const = 0;
    virtual QByteArray read(const QString& archive, const QString& name, QString* error) const = 0;
};

class ExtensionPlugin
{
public:
    virtual ~ExtensionPlugin() = default;
    virtual std::vector<std::unique_ptr<ArchiveFormat>> archiveFormats() = 0;
};

namespace Archives {

OFS_CORE_EXPORT void add(std::unique_ptr<ArchiveFormat> format);
OFS_CORE_EXPORT QStringList loadExtensions(const QStringList& searchDirs);

OFS_CORE_EXPORT const ArchiveFormat* formatOf(const QString& path);
OFS_CORE_EXPORT QString openFilter();

OFS_CORE_EXPORT QList<Zip::Entry> entries(const QString& archive, QString* error = nullptr);
OFS_CORE_EXPORT bool extract(const QString& archive, const QString& folder, QString* error = nullptr);
OFS_CORE_EXPORT QByteArray read(const QString& archive, const QString& name, QString* error = nullptr);

} // namespace Archives

} // namespace OFS

#define OFS_EXTENSION_PLUGIN_IID "org.openfactorystudio.ExtensionPlugin/1"
#include <QtPlugin>
Q_DECLARE_INTERFACE(OFS::ExtensionPlugin, OFS_EXTENSION_PLUGIN_IID)
