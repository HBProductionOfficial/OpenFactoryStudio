#pragma once

#include "core/CoreExport.h"

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>

namespace OFS {

class Engine;

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
    QString versionName() const;

    QStringList editorIds() const { return m_editorIds; }

    QStringList editorFiles(const QString& editorId) const;

    virtual bool isModified() const { return false; }

Q_SIGNALS:
    void nameChanged();
    void modifiedChanged(bool modified);

protected:
    void setTypeId(const QString& id) { m_typeId = id; }
    void setVersionId(const QString& id) { m_versionId = id; }
    void setEditorIds(const QStringList& ids) { m_editorIds = ids; }
    void setEditorFiles(const QString& editorId, const QStringList& files);

private:
    Engine* m_engine;
    QString m_filePath;
    QString m_name;
    QString m_typeId;
    QString m_versionId;
    QStringList m_editorIds;
    QHash<QString, QStringList> m_editorFiles;
};

} // namespace OFS
