#include "core/app/Translations.h"

#include "core/app/Languages.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QLibraryInfo>
#include <QList>
#include <QPointer>
#include <QTranslator>

namespace OFS::Translations {

namespace {

QList<QPointer<QTranslator>>& installed()
{
    static QList<QPointer<QTranslator>> list;
    return list;
}

bool load(const QString& path)
{
    auto* translator = new QTranslator(QCoreApplication::instance());
    if (!translator->load(path) || !QCoreApplication::installTranslator(translator)) {
        delete translator;
        return false;
    }
    installed() << translator;
    return true;
}

} // namespace

QString localeName(const QString& code)
{
    const QStringList parts = code.split(QLatin1Char('-'));
    if (parts.size() < 2)
        return code.toLower();
    return parts[0].toLower() + QLatin1Char('_') + parts[1].toUpper();
}

int install(const QString& code, const QStringList& searchDirs)
{
    for (const QPointer<QTranslator>& translator : std::as_const(installed())) {
        if (translator) {
            QCoreApplication::removeTranslator(translator);
            delete translator;
        }
    }
    installed().clear();
    if (code.isEmpty() || code.compare(SourceLanguage, Qt::CaseInsensitive) == 0 || !QCoreApplication::instance())
        return 0;
    int count = 0;
    // Qt's own texts first, so the program's catalogs win.
    const QString locale = localeName(code);
    const QString language = locale.section(QLatin1Char('_'), 0, 0);
    const QStringList qtDirs = {QCoreApplication::applicationDirPath() + QStringLiteral("/translations"),
                                QLibraryInfo::path(QLibraryInfo::TranslationsPath)};
    for (const QString& dir : qtDirs) {
        const QString full = dir + QStringLiteral("/qtbase_") + locale + QStringLiteral(".qm");
        const QString shortName = dir + QStringLiteral("/qtbase_") + language + QStringLiteral(".qm");
        if ((QFileInfo::exists(full) && load(full)) || (QFileInfo::exists(shortName) && load(shortName))) {
            ++count;
            break;
        }
    }
    // The last installed catalog is searched first, so the earlier folders go last.
    for (auto it = searchDirs.crbegin(); it != searchDirs.crend(); ++it) {
        const QDir dir(*it + QStringLiteral("/languages/") + code);
        for (const QString& name : dir.entryList({QStringLiteral("*.qm")}, QDir::Files, QDir::Name)) {
            if (load(dir.filePath(name)))
                ++count;
        }
    }
    return count;
}

} // namespace OFS::Translations
