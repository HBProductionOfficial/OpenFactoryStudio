#pragma once

#include <QLatin1StringView>
#include <QString>

#include <iterator>

namespace OFS::Mugen {

// In the order of Runtime::Redirect. With `takesId` an ID in parentheses may follow.
struct RedirectKeyword
{
    QLatin1StringView name;
    bool takesId;
};
inline constexpr RedirectKeyword RedirectKeywords[] = {
    {QLatin1StringView("parent"), false},   {QLatin1StringView("root"), false},    {QLatin1StringView("helper"), true},
    {QLatin1StringView("target"), true},    {QLatin1StringView("partner"), false}, {QLatin1StringView("enemy"), true},
    {QLatin1StringView("enemynear"), true}, {QLatin1StringView("playerid"), true},
};

inline int redirectIndex(const QString& name)
{
    for (int i = 0; i < int(std::size(RedirectKeywords)); ++i) {
        if (name == RedirectKeywords[i].name)
            return i;
    }
    return -1;
}

} // namespace OFS::Mugen
