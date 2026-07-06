#include "core/settings/Config.h"

#include "core/settings/XmlRecordFile.h"

#include <QFileInfo>

namespace OFS {

namespace {

const QString kRoot = QStringLiteral("configuration");
const QString kElement = QStringLiteral("config");
const QString kName = QStringLiteral("name");
const QString kValue = QStringLiteral("value");

QMap<QString, QString> readValues(const QString& path)
{
    QMap<QString, QString> values;
    if (path.isEmpty() || !QFileInfo::exists(path))
        return values;
    const auto result = XmlRecordFile::read(path, kRoot, kElement);
    for (const XmlRecord& record : result.records) {
        const QString name = record.value(kName);
        if (!name.isEmpty())
            values.insert(name, record.value(kValue));
    }
    return values;
}

} // namespace

Config::Config(QObject* parent)
    : QObject(parent)
{}

Config::~Config() = default;

void Config::load(const QString& defaultsPath, const QString& userPath)
{
    m_defaults = readValues(defaultsPath);
    m_values = readValues(userPath);
    m_userPath = userPath;
}

bool Config::save(QString* error) const
{
    return saveAs(m_userPath, error);
}

bool Config::saveAs(const QString& path, QString* error) const
{
    // The user file holds the whole configuration, defaults included, same as Fighter Factory.
    QMap<QString, QString> all = m_defaults;
    for (auto it = m_values.cbegin(); it != m_values.cend(); ++it)
        all.insert(it.key(), it.value());

    QList<XmlRecord> records;
    records.reserve(all.size());
    for (auto it = all.cbegin(); it != all.cend(); ++it)
        records.append(XmlRecord{{kName, it.key()}, {kValue, it.value()}});
    return XmlRecordFile::write(path, kRoot, kElement, records, error);
}

bool Config::contains(const QString& key) const
{
    return m_values.contains(key) || m_defaults.contains(key);
}

bool Config::isDefault(const QString& key) const
{
    return !m_values.contains(key) || m_values.value(key) == m_defaults.value(key);
}

QStringList Config::keys() const
{
    QMap<QString, QString> all = m_defaults;
    for (auto it = m_values.cbegin(); it != m_values.cend(); ++it)
        all.insert(it.key(), it.value());
    return all.keys();
}

QString Config::value(const QString& key, const QString& fallback) const
{
    auto it = m_values.constFind(key);
    if (it != m_values.cend())
        return it.value();
    it = m_defaults.constFind(key);
    if (it != m_defaults.cend())
        return it.value();
    return fallback;
}

bool Config::boolValue(const QString& key, bool fallback) const
{
    return decodeBool(value(key), fallback);
}

int Config::intValue(const QString& key, int fallback) const
{
    bool ok = false;
    const int v = value(key).trimmed().toInt(&ok);
    return ok ? v : fallback;
}

double Config::doubleValue(const QString& key, double fallback) const
{
    bool ok = false;
    const double v = value(key).trimmed().toDouble(&ok);
    return ok ? v : fallback;
}

QRect Config::rectValue(const QString& key, const QRect& fallback) const
{
    return decodeRect(value(key), fallback);
}

QString Config::defaultValue(const QString& key) const
{
    return m_defaults.value(key);
}

void Config::setValue(const QString& key, const QString& value)
{
    if (m_values.contains(key) && m_values.value(key) == value)
        return;
    m_values.insert(key, value);
    Q_EMIT changed(key);
}

void Config::setValue(const QString& key, bool value)
{
    setValue(key, encodeBool(value));
}

void Config::setValue(const QString& key, int value)
{
    setValue(key, QString::number(value));
}

void Config::setValue(const QString& key, double value)
{
    setValue(key, QString::number(value, 'g', 15));
}

void Config::setValue(const QString& key, const QRect& value)
{
    setValue(key, encodeRect(value));
}

void Config::reset(const QString& key)
{
    if (m_values.remove(key) > 0)
        Q_EMIT changed(key);
}

void Config::resetAll()
{
    const QStringList changedKeys = m_values.keys();
    m_values.clear();
    for (const QString& key : changedKeys)
        Q_EMIT changed(key);
}

void Config::merge(const QMap<QString, QString>& values)
{
    for (auto it = values.cbegin(); it != values.cend(); ++it)
        setValue(it.key(), it.value());
}

QString Config::encodeBool(bool value)
{
    return value ? QStringLiteral("true") : QStringLiteral("false");
}

QString Config::encodeRect(const QRect& rect)
{
    return QStringLiteral("%1,%2,%3,%4").arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height());
}

bool Config::decodeBool(const QString& text, bool fallback)
{
    const QString t = text.trimmed().toLower();
    if (t == QLatin1String("true") || t == QLatin1String("1") || t == QLatin1String("yes"))
        return true;
    if (t == QLatin1String("false") || t == QLatin1String("0") || t == QLatin1String("no"))
        return false;
    return fallback;
}

QRect Config::decodeRect(const QString& text, const QRect& fallback)
{
    const QStringList parts = text.split(QLatin1Char(','));
    if (parts.size() != 4)
        return fallback;
    int v[4];
    for (int i = 0; i < 4; ++i) {
        bool ok = false;
        v[i] = parts[i].trimmed().toInt(&ok);
        if (!ok)
            return fallback;
    }
    return QRect(v[0], v[1], v[2], v[3]);
}

} // namespace OFS
