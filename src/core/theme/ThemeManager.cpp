#include "core/theme/ThemeManager.h"

#include "core/theme/StyleSheet.h"

#include <QDir>
#include <QFileInfo>

namespace OFS {

ThemeManager::ThemeManager(QObject* parent)
    : QObject(parent)
{}

void ThemeManager::setSearchDirs(const QStringList& dirs)
{
    m_searchDirs = dirs;
}

void ThemeManager::setEngineDataDirs(const QStringList& dirs)
{
    m_engineDataDirs = dirs;
}

namespace {

QStringList listFiles(const QStringList& roots, const QString& kind, const QString& suffix)
{
    QStringList names;
    for (const QString& root : roots) {
        const QDir dir(root + QLatin1Char('/') + kind);
        for (const QFileInfo& fi : dir.entryInfoList({QStringLiteral("*.") + suffix}, QDir::Files, QDir::Name)) {
            if (!names.contains(fi.completeBaseName()))
                names << fi.completeBaseName();
        }
    }
    return names;
}

QStringList listDirs(const QStringList& roots, const QString& kind, const QString& marker)
{
    QStringList names;
    for (const QString& root : roots) {
        const QDir dir(root + QLatin1Char('/') + kind);
        for (const QString& sub : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
            if (QFileInfo::exists(dir.filePath(sub + QLatin1Char('/') + marker)) && !names.contains(sub))
                names << sub;
        }
    }
    return names;
}

} // namespace

QStringList ThemeManager::availablePresets() const
{
    return listFiles(m_searchDirs, QStringLiteral("presets"), QStringLiteral("lua"));
}

QStringList ThemeManager::availableIconSets() const
{
    return listDirs(m_searchDirs, QStringLiteral("iconsets"), QStringLiteral("iconset.xml"));
}

QStringList ThemeManager::availableStyles() const
{
    return listDirs(m_searchDirs, QStringLiteral("styles"), QStringLiteral("stylesheet.css"));
}

QStringList ThemeManager::availableColorSets() const
{
    return listFiles(m_searchDirs, QStringLiteral("colorsets"), QStringLiteral("xml"));
}

QString ThemeManager::findDir(const QString& kind, const QString& name, const QString& marker) const
{
    for (const QString& root : m_searchDirs) {
        const QString dir = root + QLatin1Char('/') + kind + QLatin1Char('/') + name;
        if (QFileInfo::exists(dir + QLatin1Char('/') + marker))
            return dir;
    }
    return {};
}

QString ThemeManager::presetPath(const QString& name) const
{
    for (const QString& root : m_searchDirs) {
        const QString path = root + QStringLiteral("/presets/") + name + QStringLiteral(".lua");
        if (QFileInfo::exists(path))
            return path;
    }
    return {};
}

QString ThemeManager::colorSetPath(const QString& name) const
{
    for (const QString& root : m_searchDirs) {
        const QString path = root + QStringLiteral("/colorsets/") + name + QStringLiteral(".xml");
        if (QFileInfo::exists(path))
            return path;
    }
    return {};
}

bool ThemeManager::setIconSet(const QString& name)
{
    const QString dir = findDir(QStringLiteral("iconsets"), name, QStringLiteral("iconset.xml"));
    if (dir.isEmpty())
        return false;
    IconSet set;
    if (!set.load(dir))
        return false;
    for (const QString& engineDir : m_engineDataDirs) {
        const QString overlay = engineDir + QStringLiteral("/iconsets/") + name;
        if (QFileInfo::exists(overlay + QStringLiteral("/iconset.xml")))
            set.addOverlay(overlay);
    }
    m_iconSet = set;
    Q_EMIT iconSetChanged();
    return true;
}

bool ThemeManager::setStyle(const QString& name)
{
    const QString dir = findDir(QStringLiteral("styles"), name, QStringLiteral("stylesheet.css"));
    if (dir.isEmpty())
        return false;
    m_styleName = name;
    m_styleSheet = StyleSheet::load(dir, m_scale);
    Q_EMIT styleChanged();
    return true;
}

bool ThemeManager::setColorSet(const QString& name)
{
    const QString path = colorSetPath(name);
    if (path.isEmpty())
        return false;
    ColorSet set;
    if (!set.load(path))
        return false;
    m_colorSet = set;
    Q_EMIT colorSetChanged();
    return true;
}

} // namespace OFS
