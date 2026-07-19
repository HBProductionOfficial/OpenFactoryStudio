#include "formats/util/FileIo.h"

#include <algorithm>
#include <fstream>
#include <system_error>

namespace OFS::Formats {

namespace {

std::string describe(const std::filesystem::path& path)
{
    std::string text;
    for (const char8_t c : path.u8string())
        text.push_back(static_cast<char>(c));
    return text;
}

} // namespace

Result<Bytes> readFile(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return Error{"cannot open " + describe(path)};
    in.seekg(0, std::ios::end);
    const std::streamoff end = in.tellg();
    if (end < 0)
        return Error{"cannot determine the size of " + describe(path)};
    in.seekg(0, std::ios::beg);
    Bytes data(static_cast<std::size_t>(end));
    if (!data.empty() && !in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size())))
        return Error{"cannot read " + describe(path)};
    return data;
}

Status writeFile(const std::filesystem::path& path, ByteSpan data)
{
    std::filesystem::path temp = path;
    temp += ".ofs-tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out)
            return Error{"cannot create " + describe(temp)};
        constexpr std::size_t chunk = std::size_t{1} << 30;
        std::size_t done = 0;
        while (done < data.size()) {
            const std::size_t n = std::min(chunk, data.size() - done);
            out.write(reinterpret_cast<const char*>(data.data() + done), static_cast<std::streamsize>(n));
            done += n;
        }
        out.flush();
        if (!out)
            return Error{"cannot write " + describe(temp)};
    }
    std::error_code ec;
    std::filesystem::rename(temp, path, ec);
    if (ec) {
        // Some platforms refuse to rename over an existing file.
        std::filesystem::remove(path, ec);
        std::filesystem::rename(temp, path, ec);
        if (ec) {
            std::filesystem::remove(temp, ec);
            return Error{"cannot replace " + describe(path)};
        }
    }
    return {};
}

} // namespace OFS::Formats
