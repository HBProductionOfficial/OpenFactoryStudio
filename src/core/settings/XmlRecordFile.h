#pragma once

#include "core/CoreExport.h"

#include <QList>
#include <QPair>
#include <QString>

namespace OFS {

class OFS_CORE_EXPORT XmlRecord
{
public:
    XmlRecord() = default;
    XmlRecord(std::initializer_list<QPair<QString, QString>> attributes);

    QString value(const QString& name, const QString& fallback = {}) const;
    bool has(const QString& name) const;
    void set(const QString& name, const QString& value);

    const QList<QPair<QString, QString>>& attributes() const { return m_attributes; }

private:
    QList<QPair<QString, QString>> m_attributes;
};

// Reads and writes the flat XML files used for settings:
//
//   <root>
//       <element a="..." b="..."/>
//       ...
//   </root>
//
// Files are written atomically. An empty list is written as <root/>.
class OFS_CORE_EXPORT XmlRecordFile
{
public:
    struct Result
    {
        QList<XmlRecord> records;
        QString error;
        bool ok() const { return error.isEmpty(); }
    };

    static Result read(const QString& path, const QString& root, const QString& element);
    static Result parse(const QByteArray& data, const QString& root, const QString& element);

    static QByteArray serialize(const QString& root, const QString& element, const QList<XmlRecord>& records);
    static bool write(const QString& path, const QString& root, const QString& element, const QList<XmlRecord>& records,
                      QString* error = nullptr);
};

} // namespace OFS
