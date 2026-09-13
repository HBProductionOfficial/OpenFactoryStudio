#pragma once

#include "core/CoreExport.h"

#include <QByteArray>
#include <QString>

namespace OFS {

// Writes through a temporary file so a failed write leaves the old file whole. On Windows the
// replace fails while another program (antivirus, indexer, our file watcher) has the file open,
// so it retries for about half a second.
OFS_CORE_EXPORT bool writeFileSafely(const QString& path, const QByteArray& bytes, QString* error = nullptr);

} // namespace OFS
