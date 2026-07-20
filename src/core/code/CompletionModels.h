#pragma once

#include "core/CoreExport.h"

#include <QList>
#include <QString>
#include <QStringList>

namespace OFS {

// Word lists offered by the code editor's completion popup (an engine's completer.xml):
//   <completion>
//     <model prefix="type="><word value=" ChangeState"/>...</model>
//   </completion>
// A model applies when the text before the cursor ends with its prefix. Words are inserted as
// written, including a leading space or a closing parenthesis.
class OFS_CORE_EXPORT CompletionModels
{
public:
    struct Model
    {
        QString prefix;
        QStringList words;
    };

    bool load(const QString& filePath, QString* error = nullptr);
    void setModels(const QList<Model>& models) { m_models = models; }
    const QList<Model>& models() const { return m_models; }
    bool isEmpty() const { return m_models.isEmpty(); }

    const Model* match(const QString& text) const;

private:
    QList<Model> m_models;
};

} // namespace OFS
