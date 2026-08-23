#pragma once

#include "core/CoreExport.h"
#include "formats/sff/SffDocument.h"

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>

namespace OFS {

class CodeFile;
class Engine;
class SpriteFile;
class ProjectFile;

// An open project: a .def file and the files it refers to. Engines subclass it for their format.
class OFS_CORE_EXPORT Project : public QObject
{
    Q_OBJECT

public:
    Project(Engine* engine, const QString& filePath, QObject* parent = nullptr);
    ~Project() override;

    Engine* engine() const { return m_engine; }

    QString filePath() const { return m_filePath; }
    QString directory() const;

    // [Info] name, else the file base name, else "Project 1", "Project 2"... for new projects.
    QString name() const { return m_name; }
    void setName(const QString& name);

    QString typeId() const { return m_typeId; }
    QString typeName() const;
    QString versionId() const { return m_versionId; }
    // False when the file does not state its engine version, so version checks are skipped.
    bool isVersionDeclared() const { return m_versionDeclared; }
    QString versionName() const;

    QStringList editorIds() const { return m_editorIds; }

    QStringList editorFiles(const QString& editorId) const;

    const QList<ProjectFile*>& files() const { return m_files; }
    ProjectFile* file(const QString& path) const;
    CodeFile* codeFile(const QString& path, const QString& editorId, QString* error = nullptr);
    SpriteFile* spriteFile(const QString& path, const QString& editorId, QString* error = nullptr);
    CodeFile* addCodeFile(const QString& path, const QString& editorId, QString* error = nullptr);
    CodeFile* newCodeFile(const QString& placeholderPath, const QString& editorId);
    SpriteFile* addSpriteFile(const QString& path, const QString& editorId, QString* error = nullptr);
    SpriteFile* newSpriteFile(const QString& placeholderPath, const QString& editorId, Formats::SffVersion version);

    bool isModified() const;
    bool save(QString* error = nullptr);

Q_SIGNALS:
    void nameChanged();
    void modifiedChanged(bool modified);

protected:
    void setTypeId(const QString& id) { m_typeId = id; }
    void setVersionId(const QString& id) { m_versionId = id; }
    void setVersionDeclared(bool declared) { m_versionDeclared = declared; }
    void setEditorIds(const QStringList& ids) { m_editorIds = ids; }
    void setEditorFiles(const QString& editorId, const QStringList& files);

private:
    void adopt(ProjectFile* file);

    Engine* m_engine;
    QString m_filePath;
    QString m_name;
    QString m_typeId;
    QString m_versionId;
    bool m_versionDeclared = true;
    QStringList m_editorIds;
    QHash<QString, QStringList> m_editorFiles;
    QList<ProjectFile*> m_files;
};

} // namespace OFS
