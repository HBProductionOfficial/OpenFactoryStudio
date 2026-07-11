#include "core/settings/SettingsBundle.h"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

namespace OFS {

namespace {

const QString kRoot = QStringLiteral("settings");
const QString kConfiguration = QStringLiteral("configuration");
const QString kConfig = QStringLiteral("config");
const QString kShortcuts = QStringLiteral("shortcuts");
const QString kShortcut = QStringLiteral("shortcut");
const QString kEngine = QStringLiteral("engine");
const QString kColors = QStringLiteral("colors");
const QString kColor = QStringLiteral("color");
const QString kName = QStringLiteral("name");
const QString kValue = QStringLiteral("value");

void writeValues(QXmlStreamWriter& xml, const QMap<QString, QString>& values)
{
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        xml.writeEmptyElement(kConfig);
        xml.writeAttribute(kName, it.key());
        xml.writeAttribute(kValue, it.value());
    }
}

QMap<QString, QString> readValues(QXmlStreamReader& xml)
{
    QMap<QString, QString> values;
    while (xml.readNextStartElement()) {
        if (xml.name() == kConfig) {
            const QString name = xml.attributes().value(kName).toString();
            if (!name.isEmpty())
                values.insert(name, xml.attributes().value(kValue).toString());
        }
        xml.skipCurrentElement();
    }
    return values;
}

} // namespace

QByteArray SettingsBundle::serialize() const
{
    QByteArray data;
    QBuffer buffer(&data);
    buffer.open(QIODevice::WriteOnly);
    QXmlStreamWriter xml(&buffer);
    xml.setAutoFormatting(true);
    xml.setAutoFormattingIndent(4);
    xml.writeStartDocument();
    xml.writeStartElement(kRoot);
    xml.writeAttribute(QStringLiteral("application"), QStringLiteral("Open Factory Studio"));
    xml.writeAttribute(QStringLiteral("version"), QString::number(Version));

    xml.writeStartElement(kConfiguration);
    writeValues(xml, config);
    xml.writeEndElement();

    xml.writeStartElement(kShortcuts);
    for (auto it = shortcuts.cbegin(); it != shortcuts.cend(); ++it) {
        xml.writeEmptyElement(kShortcut);
        xml.writeAttribute(QStringLiteral("action"), it.key());
        xml.writeAttribute(QStringLiteral("keys"), it.value());
    }
    xml.writeEndElement();

    for (auto it = engines.cbegin(); it != engines.cend(); ++it) {
        xml.writeStartElement(kEngine);
        xml.writeAttribute(kName, it.key());
        writeValues(xml, it.value());
        xml.writeEndElement();
    }

    if (!colors.isEmpty()) {
        xml.writeStartElement(kColors);
        xml.writeAttribute(kName, colorSetName);
        for (const XmlRecord& record : colors) {
            xml.writeEmptyElement(kColor);
            for (const auto& attr : record.attributes())
                xml.writeAttribute(attr.first, attr.second);
        }
        xml.writeEndElement();
    }

    xml.writeEndElement();
    xml.writeEndDocument();
    return data;
}

bool SettingsBundle::save(const QString& path, QString* error) const
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    file.write(serialize());
    if (!file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
}

bool SettingsBundle::parse(const QByteArray& data, QString* error)
{
    *this = SettingsBundle();
    SettingsBundle result;
    QXmlStreamReader xml(data);
    if (!xml.readNextStartElement() || xml.name() != kRoot) {
        if (error)
            *error = xml.hasError() ? xml.errorString() : QStringLiteral("not a settings file");
        return false;
    }
    while (xml.readNextStartElement()) {
        if (xml.name() == kConfiguration) {
            result.config = readValues(xml);
        } else if (xml.name() == kShortcuts) {
            while (xml.readNextStartElement()) {
                if (xml.name() == kShortcut) {
                    const QString action = xml.attributes().value(QStringLiteral("action")).toString();
                    if (!action.isEmpty() && !result.shortcuts.contains(action))
                        result.shortcuts.insert(action, xml.attributes().value(QStringLiteral("keys")).toString());
                }
                xml.skipCurrentElement();
            }
        } else if (xml.name() == kEngine) {
            const QString name = xml.attributes().value(kName).toString();
            const QMap<QString, QString> values = readValues(xml);
            if (!name.isEmpty())
                result.engines.insert(name, values);
        } else if (xml.name() == kColors) {
            result.colorSetName = xml.attributes().value(kName).toString();
            while (xml.readNextStartElement()) {
                if (xml.name() == kColor) {
                    XmlRecord record;
                    for (const QXmlStreamAttribute& attr : xml.attributes())
                        record.set(attr.name().toString(), attr.value().toString());
                    result.colors.append(record);
                }
                xml.skipCurrentElement();
            }
        } else {
            xml.skipCurrentElement();
        }
    }
    if (xml.hasError()) {
        if (error)
            *error = QStringLiteral("line %1: %2").arg(xml.lineNumber()).arg(xml.errorString());
        return false;
    }
    *this = result;
    return true;
}

bool SettingsBundle::load(const QString& path, QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = file.errorString();
        *this = SettingsBundle();
        return false;
    }
    return parse(file.readAll(), error);
}

} // namespace OFS
