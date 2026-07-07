#pragma once

#include "core/CoreExport.h"
#include "core/theme/ColorSet.h"
#include "core/theme/IconSet.h"

#include <QObject>
#include <QStringList>

namespace OFS {

// Finds themes in <userDir>/<kind>/ then <dataDir>/<kind>/, where kind is "iconsets" (folders with
// iconset.xml), "styles" (folders with stylesheet.css), "colorsets" (<name>.xml) or "presets"
// (<name>.lua).
class OFS_CORE_EXPORT ThemeManager : public QObject
{
    Q_OBJECT

public:
    explicit ThemeManager(QObject* parent = nullptr);

    void setSearchDirs(const QStringList& dirs);
    const QStringList& searchDirs() const { return m_searchDirs; }

    QStringList availablePresets() const;
    QStringList availableIconSets() const;
    QStringList availableStyles() const;
    QStringList availableColorSets() const;

    QString presetPath(const QString& name) const;
    QString colorSetPath(const QString& name) const;

    void setEngineDataDirs(const QStringList& dirs);

    bool setIconSet(const QString& name);
    bool setStyle(const QString& name);
    bool setColorSet(const QString& name);

    QString iconSetName() const { return m_iconSet.name(); }
    QString styleName() const { return m_styleName; }
    QString colorSetName() const { return m_colorSet.name(); }

    const IconSet& iconSet() const { return m_iconSet; }
    const ColorSet& colorSet() const { return m_colorSet; }
    const QString& styleSheet() const { return m_styleSheet; }

    void setScale(double scale) { m_scale = scale; }
    double scale() const { return m_scale; }

Q_SIGNALS:
    void iconSetChanged();
    void styleChanged();
    void colorSetChanged();

private:
    QString findDir(const QString& kind, const QString& name, const QString& marker) const;

    QStringList m_searchDirs;
    QStringList m_engineDataDirs;
    IconSet m_iconSet;
    ColorSet m_colorSet;
    QString m_styleName;
    QString m_styleSheet;
    double m_scale = 1.0;
};

} // namespace OFS
