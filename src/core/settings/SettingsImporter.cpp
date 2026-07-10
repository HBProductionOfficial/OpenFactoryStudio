#include "core/settings/SettingsImporter.h"

#include "core/app/AppContext.h"
#include "core/settings/Config.h"
#include "core/settings/History.h"
#include "core/settings/ShortcutSettings.h"
#include "core/settings/XmlRecordFile.h"
#include "core/theme/ThemeManager.h"

#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMap>
#include <QScreen>
#include <QStandardPaths>

namespace OFS {

namespace {

const QStringList kRectKeys = {QStringLiteral("advpal_geometry"), QStringLiteral("anim_editcode_geometry"),
                               QStringLiteral("project_run_geometry"), QStringLiteral("script_geometry")};

const QList<QPair<QString, QString>> kPointKeys = {{QStringLiteral("calculator_x"), QStringLiteral("calculator_y")},
                                                   {QStringLiteral("issues_x"), QStringLiteral("issues_y")}};

const QString kWindowLeft = QStringLiteral("window_left");
const QString kWindowTop = QStringLiteral("window_top");
const QString kWindowWidth = QStringLiteral("window_width");
const QString kWindowHeight = QStringLiteral("window_height");

// Smallest part of a tool window that must stay on screen when only its position is stored.
constexpr QSize kMinimumVisible(200, 100);

bool isPathKey(const QString& key)
{
    if (key.endsWith(QLatin1String("_path")) || key == QLatin1String("background_file"))
        return true;
    return key.startsWith(QLatin1String("crosshair_")) && key != QLatin1String("crosshair_type");
}

bool isThemeKey(const QString& key)
{
    return key == QLatin1String("preset") || key == QLatin1String("iconset") || key == QLatin1String("stylesheet") ||
           key == QLatin1String("colorset");
}

bool toInt(const QString& text, int* value)
{
    bool ok = false;
    *value = text.trimmed().toInt(&ok);
    return ok;
}

QList<XmlRecord> readRecords(const QString& path, const QString& root, const QString& element,
                             SettingsImporter::Report& report)
{
    if (!QFileInfo::exists(path))
        return {};
    const auto result = XmlRecordFile::read(path, root, element);
    if (!result.ok()) {
        report.errors << result.error;
        return {};
    }
    return result.records;
}

QMap<QString, QString> readConfigFile(const QString& path, SettingsImporter::Report& report)
{
    QMap<QString, QString> values;
    const auto records = readRecords(path, QStringLiteral("configuration"), QStringLiteral("config"), report);
    for (const XmlRecord& record : records) {
        const QString name = record.value(QStringLiteral("name"));
        if (!name.isEmpty())
            values.insert(name, record.value(QStringLiteral("value")));
    }
    return values;
}

} // namespace

SettingsImporter::SettingsImporter(const QString& sourceDir)
    : m_sourceDir(QDir::cleanPath(sourceDir))
{
    if (qobject_cast<QGuiApplication*>(QCoreApplication::instance())) {
        const auto screens = QGuiApplication::screens();
        for (const QScreen* screen : screens)
            m_screens << screen->availableGeometry();
    }
}

QString SettingsImporter::defaultSourceDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) +
           QStringLiteral("/Fighter Factory Studio");
}

bool SettingsImporter::hasSettings() const
{
    return QFileInfo::exists(m_sourceDir + QStringLiteral("/config.xml")) ||
           QFileInfo::exists(m_sourceDir + QStringLiteral("/shortcuts.xml"));
}

void SettingsImporter::setAvailableThemes(const QStringList& presets, const QStringList& iconSets,
                                          const QStringList& styles, const QStringList& colorSets)
{
    m_presets = presets;
    m_iconSets = iconSets;
    m_styles = styles;
    m_colorSets = colorSets;
}

SettingsImporter::Report SettingsImporter::import(Config& config, ShortcutSettings& shortcuts, History& history,
                                                  Config* engineConfig, Parts parts) const
{
    Report report;
    if (parts & ConfigPart) {
        importConfig(config, report);
        history.setMaxEntries(config.intValue(QStringLiteral("max_history"), history.maxEntries()));
    }
    if (parts & ShortcutsPart)
        importShortcuts(shortcuts, report);
    if (parts & HistoryPart)
        importHistory(history, report);
    if ((parts & EnginePart) && engineConfig)
        importEngine(*engineConfig, report);
    return report;
}

SettingsImporter::Report SettingsImporter::import(AppContext& context, Parts parts)
{
    const ThemeManager& theme = context.theme();
    setAvailableThemes(theme.availablePresets(), theme.availableIconSets(), theme.availableStyles(),
                       theme.availableColorSets());
    return import(context.config(), context.shortcuts(), context.history(),
                  &context.engineConfig(QStringLiteral("mugen")), parts);
}

void SettingsImporter::importConfig(Config& config, Report& report) const
{
    QMap<QString, QString> values = readConfigFile(m_sourceDir + QStringLiteral("/config.xml"), report);

    int left = 0;
    int top = 0;
    int width = 0;
    int height = 0;
    if (toInt(values.value(kWindowLeft), &left) && toInt(values.value(kWindowTop), &top) &&
        toInt(values.value(kWindowWidth), &width) && toInt(values.value(kWindowHeight), &height)) {
        const QRect original(left, top, width, height);
        const QRect fitted = fitToScreens(original, m_screens);
        if (fitted != original) {
            report.adjustedGeometry << QStringLiteral("window");
            values.insert(kWindowLeft, QString::number(fitted.x()));
            values.insert(kWindowTop, QString::number(fitted.y()));
            values.insert(kWindowWidth, QString::number(fitted.width()));
            values.insert(kWindowHeight, QString::number(fitted.height()));
        }
    }

    for (const QString& key : kRectKeys) {
        auto it = values.find(key);
        if (it == values.end())
            continue;
        const QRect original = Config::decodeRect(it.value(), QRect());
        if (!original.isValid())
            continue;
        const QRect fitted = fitToScreens(original, m_screens);
        if (fitted != original) {
            report.adjustedGeometry << key;
            it.value() = Config::encodeRect(fitted);
        }
    }

    for (const auto& [xKey, yKey] : kPointKeys) {
        int x = 0;
        int y = 0;
        if (!toInt(values.value(xKey), &x) || !toInt(values.value(yKey), &y))
            continue;
        const QPoint fitted = fitToScreens(QPoint(x, y), m_screens, kMinimumVisible);
        if (fitted != QPoint(x, y)) {
            report.adjustedGeometry << xKey.chopped(2);
            values.insert(xKey, QString::number(fitted.x()));
            values.insert(yKey, QString::number(fitted.y()));
        }
    }

    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        const QString& key = it.key();
        const QString& value = it.value();
        if (isPathKey(key) && !value.isEmpty() && !QFileInfo::exists(value)) {
            report.droppedPaths << key + QStringLiteral(": ") + value;
            continue;
        }
        if (isThemeKey(key) && !themeInstalled(key, value)) {
            report.skippedThemes << key + QStringLiteral(": ") + value;
            continue;
        }
        config.setValue(key, value);
        ++report.configValues;
    }
}

void SettingsImporter::importShortcuts(ShortcutSettings& shortcuts, Report& report) const
{
    const auto records = readRecords(m_sourceDir + QStringLiteral("/shortcuts.xml"), QStringLiteral("shortcuts"),
                                     QStringLiteral("shortcut"), report);
    QStringList seen;
    for (const XmlRecord& record : records) {
        const QString action = record.value(QStringLiteral("action"));
        // A repeated action would be an extra binding; only one sequence per action is kept.
        if (action.isEmpty() || seen.contains(action))
            continue;
        seen << action;
        const QString text = record.value(QStringLiteral("keys")).trimmed();
        const QKeySequence keys = ShortcutSettings::fromText(text);
        if (!text.isEmpty() && keys.isEmpty()) {
            report.errors << QStringLiteral("shortcuts.xml: cannot read the keys \"%1\" of %2").arg(text, action);
            continue;
        }
        shortcuts.setKeys(action, keys);
        ++report.shortcuts;
    }
}

void SettingsImporter::importHistory(History& history, Report& report) const
{
    const auto records = readRecords(m_sourceDir + QStringLiteral("/history.xml"), QStringLiteral("history"),
                                     QStringLiteral("entry"), report);
    QList<History::Entry> entries = history.entries();
    auto contains = [&entries](const QString& file) {
        for (const History::Entry& entry : std::as_const(entries)) {
            if (QFileInfo(entry.file) == QFileInfo(file))
                return true;
        }
        return false;
    };
    for (const XmlRecord& record : records) {
        const QString file = QDir::fromNativeSeparators(record.value(QStringLiteral("file")));
        if (file.isEmpty())
            continue;
        if (!QFileInfo::exists(file)) {
            report.droppedPaths << file;
            continue;
        }
        if (contains(file))
            continue;
        entries.append({record.value(QStringLiteral("name")), file});
        ++report.historyEntries;
    }
    history.setEntries(entries);
}

void SettingsImporter::importEngine(Config& engineConfig, Report& report) const
{
    const QMap<QString, QString> values = readConfigFile(m_sourceDir + QStringLiteral("/mugen_config.xml"), report);
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        if (isPathKey(it.key()) && !it.value().isEmpty() && !QFileInfo::exists(it.value())) {
            report.droppedPaths << QStringLiteral("mugen ") + it.key() + QStringLiteral(": ") + it.value();
            continue;
        }
        engineConfig.setValue(it.key(), it.value());
        ++report.engineValues;
    }
}

bool SettingsImporter::themeInstalled(const QString& key, const QString& name) const
{
    const QStringList* list = nullptr;
    if (key == QLatin1String("preset"))
        list = &m_presets;
    else if (key == QLatin1String("iconset"))
        list = &m_iconSets;
    else if (key == QLatin1String("stylesheet"))
        list = &m_styles;
    else
        list = &m_colorSets;
    return list->isEmpty() || list->contains(name);
}

QRect SettingsImporter::fitToScreens(const QRect& rect, const QList<QRect>& screens)
{
    if (screens.isEmpty() || !rect.isValid())
        return rect;
    for (const QRect& screen : screens) {
        if (screen.contains(rect))
            return rect;
    }

    qsizetype best = -1;
    qint64 bestArea = 0;
    for (qsizetype i = 0; i < screens.size(); ++i) {
        const QRect part = screens[i].intersected(rect);
        const qint64 area = qint64(part.width()) * part.height();
        if (area > bestArea) {
            bestArea = area;
            best = i;
        }
    }
    if (best < 0) {
        int bestDistance = 0;
        for (qsizetype i = 0; i < screens.size(); ++i) {
            const int distance = (screens[i].center() - rect.center()).manhattanLength();
            if (best < 0 || distance < bestDistance) {
                bestDistance = distance;
                best = i;
            }
        }
    }

    const QRect& screen = screens[best];
    const int width = qMin(rect.width(), screen.width());
    const int height = qMin(rect.height(), screen.height());
    const int x = qBound(screen.left(), rect.x(), screen.left() + screen.width() - width);
    const int y = qBound(screen.top(), rect.y(), screen.top() + screen.height() - height);
    return QRect(x, y, width, height);
}

QPoint SettingsImporter::fitToScreens(const QPoint& pos, const QList<QRect>& screens, const QSize& minimumSize)
{
    return fitToScreens(QRect(pos, minimumSize), screens).topLeft();
}

} // namespace OFS
