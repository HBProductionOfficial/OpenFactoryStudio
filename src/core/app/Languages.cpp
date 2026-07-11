#include "core/app/Languages.h"

#include "core/Version.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QXmlStreamReader>

#include <algorithm>

namespace OFS {

LanguageInfo LanguageInfo::read(const QString& dir)
{
    LanguageInfo info;
    QFile file(dir + QStringLiteral("/info.xml"));
    if (!file.open(QIODevice::ReadOnly))
        return info;
    QXmlStreamReader xml(&file);
    if (!xml.readNextStartElement() || xml.name() != QLatin1String("language"))
        return info;
    while (xml.readNextStartElement()) {
        const QString value = xml.attributes().value(QStringLiteral("value")).toString();
        if (xml.name() == QLatin1String("name"))
            info.name = value;
        else if (xml.name() == QLatin1String("author"))
            info.author = value;
        else if (xml.name() == QLatin1String("contact"))
            info.contact = value;
        xml.skipCurrentElement();
    }
    if (xml.hasError())
        return {};
    info.code = QFileInfo(dir).fileName();
    if (info.name.isEmpty())
        info.name = info.code;
    return info;
}

QList<LanguageInfo> LanguageInfo::available(const QStringList& searchDirs)
{
    QList<LanguageInfo> list;
    auto has = [&list](const QString& wanted) {
        return std::any_of(list.cbegin(), list.cend(), [&wanted](const LanguageInfo& l) { return l.code == wanted; });
    };
    for (const QString& root : searchDirs) {
        const QDir dir(root + QStringLiteral("/languages"));
        for (const QString& sub : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
            if (has(sub))
                continue;
            const LanguageInfo info = read(dir.filePath(sub));
            if (!info.code.isEmpty())
                list << info;
        }
    }
    if (!has(builtin().code))
        list << builtin();
    std::sort(list.begin(), list.end(),
              [](const LanguageInfo& a, const LanguageInfo& b) { return a.name.localeAwareCompare(b.name) < 0; });
    return list;
}

LanguageInfo LanguageInfo::builtin()
{
    return {QStringLiteral("en-us"), QStringLiteral("English (USA)"),
            QStringLiteral("Open Factory Studio contributors"), QStringLiteral(OFS_HOMEPAGE)};
}

} // namespace OFS
