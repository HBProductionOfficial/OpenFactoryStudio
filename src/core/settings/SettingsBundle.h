#pragma once

#include "core/CoreExport.h"
#include "core/settings/XmlRecordFile.h"

#include <QList>
#include <QMap>
#include <QString>

namespace OFS {

// Every user setting in one file, for Options > Save... and Options > Open...:
//
//   <settings application="Open Factory Studio" version="1">
//       <configuration><config name="font_size" value="10"/></configuration>
//       <shortcuts><shortcut action="file_open" keys="Ctrl+O"/></shortcuts>
//       <engine name="mugen"><config name="p1_left" value="A"/></engine>
//       <colors name="Custom"><color name="keyword" value="255,255,255" bold="true"/></colors>
//   </settings>
//
// Sections use the same elements as the separate settings files. Every section is optional.
class OFS_CORE_EXPORT SettingsBundle
{
public:
    static constexpr int Version = 1;
    static QString fileSuffix() { return QStringLiteral("ofsc"); }

    QMap<QString, QString> config;
    QMap<QString, QString> shortcuts;
    QMap<QString, QMap<QString, QString>> engines;
    QString colorSetName;
    QList<XmlRecord> colors;

    QByteArray serialize() const;
    bool save(const QString& path, QString* error = nullptr) const;

    bool parse(const QByteArray& data, QString* error = nullptr);
    bool load(const QString& path, QString* error = nullptr);
};

} // namespace OFS
