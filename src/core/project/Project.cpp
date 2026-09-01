#include "core/project/Project.h"

#include "core/engine/Engine.h"
#include "core/project/ProjectFile.h"
#include "core/sounds/SoundFile.h"
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
    QString n = name.trimmed();
    if (n.isEmpty() && m_memoryOnly) {
        static int s_untitledCount = 0;
        if (m_untitledName.isEmpty())
            m_untitledName = tr("Project %1").arg(++s_untitledCount);
        n = m_untitledName;
    } else if (n.isEmpty()) {
        n = QFileInfo(m_filePath).completeBaseName();
    }
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

SpriteFile* Project::addSpriteFile(const QString& path, const QString& editorId, QString* error)
{
    SpriteFile* f = spriteFile(path, editorId, error);
    if (f && !m_editorFiles.value(editorId).contains(f->path()))
        m_editorFiles[editorId] << f->path();
    return f;
}

SpriteFile* Project::newSpriteFile(const QString& placeholderPath, const QString& editorId, Formats::SffVersion version)
{
    auto* f = new SpriteFile(placeholderPath, editorId, this);
    f->createEmpty(version);
    adopt(f);
    m_editorFiles[editorId] << f->path();
    return f;
}

SoundFile* Project::soundFile(const QString& path, const QString& editorId, QString* error)
{
    if (auto* existing = qobject_cast<SoundFile*>(file(path)))
        return existing;
    auto* f = new SoundFile(path, editorId, this);
    if (!f->load(error)) {
        delete f;
        return nullptr;
    }
    adopt(f);
    return f;
}

SoundFile* Project::addSoundFile(const QString& path, const QString& editorId, QString* error)
{
    SoundFile* f = soundFile(path, editorId, error);
    if (f && !m_editorFiles.value(editorId).contains(f->path()))
        m_editorFiles[editorId] << f->path();
    return f;
}

SoundFile* Project::newSoundFile(const QString& placeholderPath, const QString& editorId)
{
    auto* f = new SoundFile(placeholderPath, editorId, this);
    f->createEmpty();
    adopt(f);
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

void Project::openAsCopy()
{
    m_memoryOnly = true;
    m_copy = true;
    for (ProjectFile* f : std::as_const(m_files))
        f->markAsCopy();
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
        if (m_memoryOnly && !f->isUntitled() && oldPath.compare(m_filePath, Qt::CaseInsensitive) == 0)
            definitionsSaved(f->path());
    });
    m_files << f;
    if (m_copy)
        f->markAsCopy();
}

void Project::definitionsSaved(const QString& path)
{
    const QString oldFolder = directory() + QLatin1Char('/');
    m_filePath = path;
    m_memoryOnly = false;
    m_copy = false;
    // Suggest the other new documents beside it, under the names the .def gives them.
    const QString newFolder = directory() + QLatin1Char('/');
    for (ProjectFile* f : std::as_const(m_files)) {
        if (f->isUntitled() && f->path().startsWith(oldFolder, Qt::CaseInsensitive))
            f->setPlaceholderPath(newFolder + f->path().mid(oldFolder.size()));
    }
    Q_EMIT filePathChanged();
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
