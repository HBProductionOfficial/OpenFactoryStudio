#include "core/theme/ColorSet.h"

#include "core/settings/Config.h"
#include "core/settings/XmlRecordFile.h"
#include "core/theme/ColorRoles.h"

#include <QFileInfo>

namespace OFS {

namespace {

const QString kRoot = QStringLiteral("colors");
const QString kElement = QStringLiteral("color");

QColor parseRgb(const QString& text)
{
    const QStringList parts = text.split(QLatin1Char(','));
    if (parts.size() < 3)
        return {};
    int v[4] = {0, 0, 0, 255};
    for (qsizetype i = 0; i < parts.size() && i < 4; ++i) {
        bool ok = false;
        v[i] = qBound(0, parts[i].trimmed().toInt(&ok), 255);
        if (!ok)
            return {};
    }
    return QColor(v[0], v[1], v[2], v[3]);
}

QString formatRgb(const QColor& c)
{
    QString s = QStringLiteral("%1,%2,%3").arg(c.red()).arg(c.green()).arg(c.blue());
    if (c.alpha() != 255)
        s += QStringLiteral(",%1").arg(c.alpha());
    return s;
}

} // namespace

bool ColorSet::load(const QString& path, QString* error)
{
    const auto result = XmlRecordFile::read(path, kRoot, kElement);
    if (!result.ok()) {
        if (error)
            *error = result.error;
        return false;
    }
    m_name = QFileInfo(path).completeBaseName();
    setRecords(result.records);
    return true;
}

bool ColorSet::save(const QString& path, QString* error) const
{
    return XmlRecordFile::write(path, kRoot, kElement, toRecords(), error);
}

QList<XmlRecord> ColorSet::toRecords() const
{
    QList<XmlRecord> records;
    for (auto it = m_entries.cbegin(); it != m_entries.cend(); ++it) {
        XmlRecord record{{QStringLiteral("name"), it.key()}, {QStringLiteral("value"), formatRgb(it->color)}};
        if (it->bold)
            record.set(QStringLiteral("bold"), QStringLiteral("true"));
        if (it->italic)
            record.set(QStringLiteral("italic"), QStringLiteral("true"));
        records.append(record);
    }
    return records;
}

void ColorSet::setRecords(const QList<XmlRecord>& records)
{
    m_entries.clear();
    for (const XmlRecord& record : records) {
        const QString role = record.value(QStringLiteral("name"));
        const QColor color = parseRgb(record.value(QStringLiteral("value")));
        if (role.isEmpty() || !color.isValid())
            continue;
        Entry entry;
        entry.color = color;
        entry.bold = Config::decodeBool(record.value(QStringLiteral("bold")), false);
        entry.italic = Config::decodeBool(record.value(QStringLiteral("italic")), false);
        m_entries.insert(role, entry);
    }
}

QColor ColorSet::color(const QString& role, const QColor& fallback) const
{
    auto it = m_entries.constFind(role);
    if (it != m_entries.cend())
        return it->color;
    if (fallback.isValid())
        return fallback;
    const ColorRole* known = ColorRole::find(role);
    return known ? known->fallback : QColor();
}

} // namespace OFS
