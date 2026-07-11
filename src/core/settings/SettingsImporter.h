#pragma once

#include "core/CoreExport.h"

#include <QFlags>
#include <QList>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QString>
#include <QStringList>

namespace OFS {

class AppContext;
class Config;
class History;
class ShortcutSettings;

// "Import from previous installation": merges Fighter Factory Studio's settings folder into ours.
// Its files use the same formats, keys and action ids. Paths that no longer exist are dropped and
// window geometry is fitted to the screens. Themes that are not installed are skipped.
class OFS_CORE_EXPORT SettingsImporter
{
public:
    enum Part
    {
        ConfigPart = 0x1,
        ShortcutsPart = 0x2,
        HistoryPart = 0x4,
        EnginePart = 0x8,
        AllParts = 0xF,
    };
    Q_DECLARE_FLAGS(Parts, Part)

    struct Report
    {
        int configValues = 0;
        int shortcuts = 0;
        int historyEntries = 0;
        int engineValues = 0;
        QStringList droppedPaths;
        QStringList adjustedGeometry;
        QStringList skippedThemes;
        QStringList errors;

        bool ok() const { return errors.isEmpty(); }
        int total() const { return configValues + shortcuts + historyEntries + engineValues; }
    };

    explicit SettingsImporter(const QString& sourceDir = defaultSourceDir());

    // %LOCALAPPDATA%/Fighter Factory Studio on Windows, the generic data folder elsewhere.
    static QString defaultSourceDir();

    QString sourceDir() const { return m_sourceDir; }
    bool hasSettings() const;

    void setScreens(const QList<QRect>& availableGeometries) { m_screens = availableGeometries; }
    const QList<QRect>& screens() const { return m_screens; }

    void setAvailableThemes(const QStringList& presets, const QStringList& iconSets, const QStringList& styles,
                            const QStringList& colorSets);

    Report import(Config& config, ShortcutSettings& shortcuts, History& history, Config* engineConfig,
                  Parts parts = AllParts) const;
    Report import(AppContext& context, Parts parts = AllParts);

    static QRect fitToScreens(const QRect& rect, const QList<QRect>& screens);
    static QPoint fitToScreens(const QPoint& pos, const QList<QRect>& screens, const QSize& minimumSize);

private:
    void importConfig(Config& config, Report& report) const;
    void importShortcuts(ShortcutSettings& shortcuts, Report& report) const;
    void importHistory(History& history, Report& report) const;
    void importEngine(Config& engineConfig, Report& report) const;
    bool themeInstalled(const QString& key, const QString& name) const;

    QString m_sourceDir;
    QList<QRect> m_screens;
    QStringList m_presets;
    QStringList m_iconSets;
    QStringList m_styles;
    QStringList m_colorSets;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(SettingsImporter::Parts)

} // namespace OFS
