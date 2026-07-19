#pragma once

// OFS_CORPUS_KFM: the stock Kung Fu Man files. OFS_CORPUS_MUGEN: a M.U.G.E.N 1.0 folder with
// sff2png.exe, sprmake2.exe and sndmaker.exe. Tests that need them skip without them.

#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>

namespace OFS::Formats::Test {

inline std::optional<std::string> envVar(const char* name)
{
#if defined(_MSC_VER)
    char* value = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&value, &length, name) != 0 || value == nullptr)
        return std::nullopt;
    std::string result(value);
    std::free(value);
#else
    const char* value = std::getenv(name);
    if (value == nullptr)
        return std::nullopt;
    std::string result(value);
#endif
    if (result.empty())
        return std::nullopt;
    return result;
}

inline std::optional<std::filesystem::path> corpusDir(const char* variable)
{
    const std::optional<std::string> value = envVar(variable);
    if (!value)
        return std::nullopt;
    // Environment strings use the native narrow encoding, which is what path(std::string) expects.
    std::filesystem::path path(*value);
    std::error_code ec;
    if (!std::filesystem::is_directory(path, ec))
        return std::nullopt;
    return path;
}

} // namespace OFS::Formats::Test
