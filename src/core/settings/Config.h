#pragma once

#include "core/CoreExport.h"

#include <QMap>
#include <QObject>
#include <QRect>
#include <QString>
#include <QStringList>

namespace OFS {

// Application settings (config.xml), stored as strings exactly as in the file:
//     <config name="zoom_level" value="1"/>
// Defaults come from the shipped data/config.xml and the user's file only overrides them.
// Unknown keys are kept.
class OFS_CORE_EXPORT Config : public QObject
{
    Q_OBJECT

public:
    explicit Config(QObject* parent = nullptr);
    ~Config() override;

    void load(const QString& defaultsPath, const QString& userPath);
    bool save(QString* error = nullptr) const;
    bool saveAs(const QString& path, QString* error = nullptr) const;

    QString userPath() const { return m_userPath; }

    bool contains(const QString& key) const;
    bool isDefault(const QString& key) const;
    QStringList keys() const;

    QString value(const QString& key, const QString& fallback = {}) const;
    bool boolValue(const QString& key, bool fallback = false) const;
    int intValue(const QString& key, int fallback = 0) const;
    double doubleValue(const QString& key, double fallback = 0.0) const;
    QRect rectValue(const QString& key, const QRect& fallback = {}) const;
    QString defaultValue(const QString& key) const;

    void setValue(const QString& key, const QString& value);
    void setValue(const QString& key, const char* value) { setValue(key, QString::fromUtf8(value)); }
    void setValue(const QString& key, bool value);
    void setValue(const QString& key, int value);
    void setValue(const QString& key, double value);
    void setValue(const QString& key, const QRect& value);

    void reset(const QString& key);
    void resetAll();
    void merge(const QMap<QString, QString>& values);

    const QMap<QString, QString>& userValues() const { return m_values; }

    static QString encodeBool(bool value);
    static QString encodeRect(const QRect& rect);
    static bool decodeBool(const QString& text, bool fallback);
    static QRect decodeRect(const QString& text, const QRect& fallback);

Q_SIGNALS:
    void changed(const QString& key);

private:
    QMap<QString, QString> m_defaults;
    QMap<QString, QString> m_values;
    QString m_userPath;
};

} // namespace OFS
