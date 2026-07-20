#include "core/code/CompletionModels.h"

#include <QFile>
#include <QXmlStreamReader>

namespace OFS {

namespace {

QString withoutSpaces(const QString& text)
{
    QString result;
    result.reserve(text.size());
    for (QChar c : text) {
        if (!c.isSpace())
            result += c;
    }
    return result;
}

} // namespace

bool CompletionModels::load(const QString& filePath, QString* error)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    QXmlStreamReader xml(&file);
    if (!xml.readNextStartElement() || xml.name() != QLatin1String("completion")) {
        if (error)
            *error = xml.hasError() ? xml.errorString() : QStringLiteral("not a completion file");
        return false;
    }
    QList<Model> models;
    while (xml.readNextStartElement()) {
        if (xml.name() != QLatin1String("model")) {
            xml.skipCurrentElement();
            continue;
        }
        Model model;
        model.prefix = xml.attributes().value(QLatin1String("prefix")).toString();
        while (xml.readNextStartElement()) {
            if (xml.name() == QLatin1String("word"))
                model.words << xml.attributes().value(QLatin1String("value")).toString();
            xml.skipCurrentElement();
        }
        if (!model.prefix.isEmpty() && !model.words.isEmpty())
            models << model;
    }
    if (xml.hasError()) {
        if (error)
            *error = QStringLiteral("%1:%2: %3").arg(xml.lineNumber()).arg(xml.columnNumber()).arg(xml.errorString());
        return false;
    }
    m_models = models;
    return true;
}

const CompletionModels::Model* CompletionModels::match(const QString& text) const
{
    const QString compact = withoutSpaces(text);
    for (const Model& model : m_models) {
        if (compact.endsWith(withoutSpaces(model.prefix), Qt::CaseInsensitive))
            return &model;
    }
    return nullptr;
}

} // namespace OFS
