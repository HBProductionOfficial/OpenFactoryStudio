#include "core/settings/ShortcutSettings.h"

#include "core/settings/XmlRecordFile.h"

#include <QFileInfo>

namespace OFS {

namespace {

const QString kRoot = QStringLiteral("shortcuts");
const QString kElement = QStringLiteral("shortcut");
const QString kAction = QStringLiteral("action");
const QString kKeys = QStringLiteral("keys");

QMap<QString, QString> readTable(const QString& path)
{
    QMap<QString, QString> table;
    if (path.isEmpty() || !QFileInfo::exists(path))
        return table;
    const auto result = XmlRecordFile::read(path, kRoot, kElement);
    for (const XmlRecord& record : result.records) {
        const QString action = record.value(kAction);
        if (!action.isEmpty())
            table.insert(action, record.value(kKeys));
    }
    return table;
}

} // namespace

ShortcutSettings::ShortcutSettings(QObject* parent)
    : QObject(parent)
{}

void ShortcutSettings::load(const QString& defaultsPath, const QString& userPath)
{
    m_defaults = readTable(defaultsPath);
    m_values = readTable(userPath);
    m_userPath = userPath;
}

bool ShortcutSettings::save(QString* error) const
{
    return saveAs(m_userPath, error);
}

bool ShortcutSettings::saveAs(const QString& path, QString* error) const
{
    QMap<QString, QString> all = m_defaults;
    for (auto it = m_values.cbegin(); it != m_values.cend(); ++it)
        all.insert(it.key(), it.value());
    QList<XmlRecord> records;
    for (auto it = all.cbegin(); it != all.cend(); ++it)
        records.append(XmlRecord{{kAction, it.key()}, {kKeys, it.value()}});
    return XmlRecordFile::write(path, kRoot, kElement, records, error);
}

QKeySequence ShortcutSettings::keys(const QString& action) const
{
    auto it = m_values.constFind(action);
    if (it != m_values.cend())
        return fromText(it.value());
    return defaultKeys(action);
}

QKeySequence ShortcutSettings::defaultKeys(const QString& action) const
{
    return fromText(m_defaults.value(action));
}

bool ShortcutSettings::hasEntry(const QString& action) const
{
    return m_values.contains(action) || m_defaults.contains(action);
}

void ShortcutSettings::setKeys(const QString& action, const QKeySequence& keys)
{
    const QString text = toText(keys);
    if (m_values.contains(action) && m_values.value(action) == text)
        return;
    m_values.insert(action, text);
    Q_EMIT changed(action);
}

void ShortcutSettings::resetAll()
{
    const QStringList changedActions = m_values.keys();
    m_values.clear();
    for (const QString& action : changedActions)
        Q_EMIT changed(action);
}

QStringList ShortcutSettings::actions() const
{
    QMap<QString, QString> all = m_defaults;
    for (auto it = m_values.cbegin(); it != m_values.cend(); ++it)
        all.insert(it.key(), it.value());
    return all.keys();
}

QString ShortcutSettings::toText(const QKeySequence& keys)
{
    return keys.toString(QKeySequence::PortableText);
}

QKeySequence ShortcutSettings::fromText(const QString& text)
{
    return QKeySequence::fromString(text, QKeySequence::PortableText);
}

} // namespace OFS
