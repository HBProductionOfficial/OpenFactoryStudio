#include "core/project/Project.h"

#include "core/engine/Engine.h"
#include "core/project/ProjectFile.h"
#include "core/sprites/SpriteFile.h"

#include <QDir>
#include <QFileInfo>

namespace OFS {

Project::Project(Engine* engine, const QString& filePath, QObject* parent)
    : QObject(parent)
    , m_engine(engine)
    , m_filePath(QDir::fromNativeSeparators(QFileInfo(filePath).absoluteFilePath()))
    , m_name(QFileInfo(filePath).completeBaseName())
{}

Project::~Project()
{
    for (ProjectFile* f : std::as_const(m_files))
        disconnect(f, nullptr, this, nullptr);
    qDeleteAll(m_files);
    m_files.clear();
}

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

ProjectFile* Project::file(const QString& path) const
{
    const QString p = QDir::fromNativeSeparators(path);
    for (ProjectFile* f : m_files) {
        if (f->path().compare(p, Qt::CaseInsensitive) == 0)
            return f;
    }
    return nullptr;
}

CodeFile* Project::codeFile(const QString& path, const QString& editorId, QString* error)
{
    if (auto* existing = qobject_cast<CodeFile*>(file(path)))
        return existing;
    auto* f = new CodeFile(path, editorId, this);
    if (!f->load(error)) {
        delete f;
        return nullptr;
    }
    adopt(f);
    return f;
}

SpriteFile* Project::spriteFile(const QString& path, const QString& editorId, QString* error)
{
    if (auto* existing = qobject_cast<SpriteFile*>(file(path)))
        return existing;
    auto* f = new SpriteFile(path, editorId, this);
    if (!f->load(error)) {
        delete f;
        return nullptr;
    }
    adopt(f);
    return f;
}

CodeFile* Project::addCodeFile(const QString& path, const QString& editorId, QString* error)
{
    CodeFile* f = codeFile(path, editorId, error);
    if (f && !m_editorFiles.value(editorId).contains(f->path()))
        m_editorFiles[editorId] << f->path();
    return f;
}

CodeFile* Project::newCodeFile(const QString& placeholderPath, const QString& editorId)
{
    auto* f = new CodeFile(placeholderPath, editorId, this);
    f->createEmpty();
    adopt(f);
    m_editorFiles[editorId] << f->path();
    return f;
}

void Project::adopt(ProjectFile* f)
{
    connect(f, &ProjectFile::modifiedChanged, this, [this] { Q_EMIT modifiedChanged(isModified()); });
    connect(f, &ProjectFile::pathChanged, this, [this, f](const QString& oldPath) {
        for (QStringList& paths : m_editorFiles) {
            for (QString& path : paths) {
                if (path == oldPath)
                    path = f->path();
            }
        }
    });
    m_files << f;
}

bool Project::isModified() const
{
    for (const ProjectFile* f : m_files) {
        if (f->isModified())
            return true;
    }
    return false;
}

bool Project::save(QString* error)
{
    for (ProjectFile* f : m_files) {
        if (f->isModified() && !f->isUntitled() && !f->save(error))
            return false;
    }
    return true;
}

void Project::setEditorFiles(const QString& editorId, const QStringList& files)
{
    m_editorFiles.insert(editorId, files);
}

} // namespace OFS
