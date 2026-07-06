#include "core/settings/XmlRecordFile.h"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

namespace OFS {

XmlRecord::XmlRecord(std::initializer_list<QPair<QString, QString>> attributes)
    : m_attributes(attributes)
{}

QString XmlRecord::value(const QString& name, const QString& fallback) const
{
    for (const auto& attr : m_attributes) {
        if (attr.first == name)
            return attr.second;
    }
    return fallback;
}

bool XmlRecord::has(const QString& name) const
{
    for (const auto& attr : m_attributes) {
        if (attr.first == name)
            return true;
    }
    return false;
}

void XmlRecord::set(const QString& name, const QString& value)
{
    for (auto& attr : m_attributes) {
        if (attr.first == name) {
            attr.second = value;
            return;
        }
    }
    m_attributes.append({name, value});
}

XmlRecordFile::Result XmlRecordFile::read(const QString& path, const QString& root, const QString& element)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        Result result;
        result.error = file.errorString();
        return result;
    }
    Result result = parse(file.readAll(), root, element);
    if (!result.ok())
        result.error = QStringLiteral("%1: %2").arg(QDir::toNativeSeparators(path), result.error);
    return result;
}

XmlRecordFile::Result XmlRecordFile::parse(const QByteArray& data, const QString& root, const QString& element)
{
    Result result;
    QXmlStreamReader xml(data);
    if (!xml.readNextStartElement()) {
        result.error = xml.hasError() ? xml.errorString() : QStringLiteral("empty document");
        return result;
    }
    if (xml.name() != root) {
        result.error = QStringLiteral("expected <%1>, found <%2>").arg(root, xml.name().toString());
        return result;
    }
    while (xml.readNextStartElement()) {
        if (xml.name() == element) {
            XmlRecord record;
            for (const QXmlStreamAttribute& attr : xml.attributes())
                record.set(attr.name().toString(), attr.value().toString());
            result.records.append(record);
        }
        xml.skipCurrentElement();
    }
    if (xml.hasError())
        result.error = QStringLiteral("line %1: %2").arg(xml.lineNumber()).arg(xml.errorString());
    return result;
}

QByteArray XmlRecordFile::serialize(const QString& root, const QString& element, const QList<XmlRecord>& records)
{
    QByteArray data;
    QBuffer buffer(&data);
    buffer.open(QIODevice::WriteOnly);
    QXmlStreamWriter xml(&buffer);
    xml.setAutoFormatting(true);
    xml.setAutoFormattingIndent(4);
    xml.writeStartDocument();
    if (records.isEmpty()) {
        xml.writeEmptyElement(root);
    } else {
        xml.writeStartElement(root);
        for (const XmlRecord& record : records) {
            xml.writeEmptyElement(element);
            for (const auto& attr : record.attributes())
                xml.writeAttribute(attr.first, attr.second);
        }
        xml.writeEndElement();
    }
    xml.writeEndDocument();
    // QXmlStreamWriter ends an empty root element with an extra line break.
    while (data.endsWith(QByteArrayLiteral("\n\n")))
        data.chop(1);
    return data;
}

bool XmlRecordFile::write(const QString& path, const QString& root, const QString& element,
                          const QList<XmlRecord>& records, QString* error)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    file.write(serialize(root, element, records));
    if (!file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
}

} // namespace OFS
