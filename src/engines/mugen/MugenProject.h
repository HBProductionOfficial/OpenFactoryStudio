#pragma once

#include "core/project/Project.h"

namespace OFS::Mugen {

class DefFile;

class MugenProject : public Project
{
    Q_OBJECT

public:
    MugenProject(Engine* engine, const QString& filePath, QObject* parent = nullptr);

    bool load(QString* error);

    // Exposed for tests.
    static QString detectType(const DefFile& def);
    static QString detectVersion(const DefFile& def);
    static QStringList editorsForType(const QString& typeId);

private:
    QString resolve(const QString& relative) const;
    void collectFiles(const DefFile& def);
};

} // namespace OFS::Mugen
