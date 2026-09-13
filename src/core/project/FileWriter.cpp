#include "core/project/FileWriter.h"

#include <QDir>
#include <QFileInfo>
#include <QSaveFile>
#include <QThread>

namespace OFS {

bool writeFileSafely(const QString& path, const QByteArray& bytes, QString* error)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QString reason;
    for (int attempt = 0; attempt < 12; ++attempt) {
        if (attempt > 0)
            QThread::msleep(std::min(20 * attempt, 80));
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size()) {
            if (error)
                *error = file.errorString();
            return false;
        }
        if (file.commit())
            return true;
        reason = file.errorString();
    }
    if (error)
        *error = reason;
    return false;
}

} // namespace OFS
