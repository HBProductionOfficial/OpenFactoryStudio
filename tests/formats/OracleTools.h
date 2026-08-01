#pragma once

#include "formats/TestEnv.h"
#include "formats/util/FileIo.h"

#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>

namespace OFS::Formats::Test {

inline std::string nativePath(const std::filesystem::path& path)
{
    return path.string();
}

inline std::optional<std::filesystem::path> mugenTool(const char* name)
{
#if defined(_WIN32)
    const auto mugen = corpusDir("OFS_CORPUS_MUGEN");
    if (!mugen)
        return std::nullopt;
    const std::filesystem::path exe = *mugen / name;
    std::error_code ec;
    if (!std::filesystem::is_regular_file(exe, ec))
        return std::nullopt;
    return exe;
#else
    (void)name;
    return std::nullopt;
#endif
}

inline std::filesystem::path freshOracleDir(const std::string& name)
{
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "ofs-oracle" / name;
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir);
    return dir;
}

// `args` may hold redirections such as "< input.txt".
inline int runTool(const std::filesystem::path& dir, const std::filesystem::path& exe, const std::string& args)
{
    // cmd.exe drops the outermost quotes of the command, hence the extra pair around everything.
    const std::string command = "\"cd /d \"" + nativePath(dir) + "\" && \"" + nativePath(exe) + "\" " + args + " > \"" +
                                nativePath(dir / "log.txt") + "\" 2>&1\"";
    return std::system(command.c_str());
}

inline std::string readToolLog(const std::filesystem::path& dir)
{
    auto text = readFile(dir / "log.txt");
    return text ? toText(*text) : std::string();
}

} // namespace OFS::Formats::Test
