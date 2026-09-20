#pragma once

#include "core/CoreExport.h"

#include <QList>
#include <QString>
#include <QStringList>

namespace OFS {

// Language of the source texts and the default `language` setting.
inline constexpr QLatin1StringView SourceLanguage{"en-us"};

// An interface language, declared by languages/<code>/info.xml with <name>, <author> and <contact>
// elements that hold a value attribute. The folder name is the `language` setting.
struct OFS_CORE_EXPORT LanguageInfo
{
    QString code;
    QString name;
    QString author;
    QString contact;

    bool operator==(const LanguageInfo&) const = default;

    static LanguageInfo read(const QString& dir);

    // Earlier roots win. English (USA) is always there because the source texts use it.
    static QList<LanguageInfo> available(const QStringList& searchDirs);

    static LanguageInfo builtin();
};

} // namespace OFS
