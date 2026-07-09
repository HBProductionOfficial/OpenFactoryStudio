#include "core/project/Project.h"

#include "core/engine/Engine.h"

#include <QDir>
#include <QFileInfo>

namespace OFS {

Project::Project(Engine* engine, const QString& filePath, QObject* parent)
    : QObject(parent)
    , m_engine(engine)
    , m_filePath(QDir::fromNativeSeparators(QFileInfo(filePath).absoluteFilePath()))
    , m_name(QFileInfo(filePath).completeBaseName())
{}

Project::~Project() = default;

QString Project::directory() const
{
    return QFileInfo(m_filePath).absolutePath();
}

void Project::setName(const QString& name)
{
    const QString n = name.trimmed().isEmpty() ? QFileInfo(m_filePath).completeBaseName() : name.trimmed();
    if (n == m_name)
        return;
    m_name = n;
    Q_EMIT nameChanged();
}

QString Project::typeName() const
{
    for (const ProjectTypeInfo& t : m_engine->projectTypes()) {
        if (t.id == m_typeId)
            return t.name;
    }
    return m_typeId;
}

QString Project::versionName() const
{
    for (const EngineVersion& v : m_engine->versions()) {
        if (v.id == m_versionId)
            return v.name;
    }
    return m_versionId;
}

QStringList Project::editorFiles(const QString& editorId) const
{
    return m_editorFiles.value(editorId);
}

void Project::setEditorFiles(const QString& editorId, const QStringList& files)
{
    m_editorFiles.insert(editorId, files);
}

} // namespace OFS
