#pragma once

#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace OFS::Formats::Maker {

inline std::string_view trim(std::string_view s)
{
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t'))
        s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t'))
        s.remove_suffix(1);
    return s;
}

inline std::string lower(std::string_view s)
{
    std::string out(s);
    for (char& c : out) {
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
    }
    return out;
}

inline std::vector<std::string_view> split(std::string_view s, char separator)
{
    std::vector<std::string_view> parts;
    std::size_t start = 0;
    for (;;) {
        const std::size_t end = s.find(separator, start);
        if (end == std::string_view::npos) {
            parts.push_back(s.substr(start));
            return parts;
        }
        parts.push_back(s.substr(start, end - start));
        start = end + 1;
    }
}

inline std::vector<std::string_view> lines(std::string_view text)
{
    std::vector<std::string_view> out = split(text, '\n');
    for (std::string_view& line : out) {
        if (!line.empty() && line.back() == '\r')
            line.remove_suffix(1);
    }
    if (!out.empty() && out.back().empty())
        out.pop_back();
    return out;
}

inline bool parseInt(std::string_view s, std::int32_t& value)
{
    s = trim(s);
    if (s.empty())
        return false;
    const auto [end, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
    return ec == std::errc() && end == s.data() + s.size();
}

} // namespace OFS::Formats::Maker
