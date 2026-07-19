#include "formats/snd/SndDocument.h"

#include "formats/util/ByteReader.h"
#include "formats/util/ByteWriter.h"
#include "formats/util/FileIo.h"

#include <algorithm>
#include <string>
#include <string_view>

namespace OFS::Formats {

namespace {

constexpr std::string_view kSignature{"ElecbyteSnd\0", 12};
constexpr std::size_t kHeaderSize = 512;
constexpr std::size_t kSubheaderSize = 16;
constexpr std::size_t kCommentOffset = 24;

} // namespace

Result<SndDocument> SndDocument::read(ByteSpan data)
{
    if (data.size() < kHeaderSize)
        return Error{"SND header is truncated"};
    if (toText(data.first(kSignature.size())) != kSignature)
        return Error{"not an SND file (signature missing)"};

    SndDocument doc;
    ByteReader r(data);
    r.seek(12);
    for (std::uint8_t& b : doc.versionBytes)
        b = r.u8();
    const std::uint32_t count = r.u32();
    const std::uint32_t first = r.u32();
    std::copy_n(asU8(data).begin() + kCommentOffset, doc.comment.size(), doc.comment.begin());

    if (count > data.size() / kSubheaderSize)
        return Error{"SND sound count is larger than the file can hold"};
    if (count > 0 && first < kCommentOffset)
        return Error{"SND first subheader offset is invalid"};

    std::uint64_t offset = first;
    doc.sounds.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        if (offset > data.size() || data.size() - offset < kSubheaderSize)
            return Error{"SND subheader " + std::to_string(i) + " lies outside the file"};
        r.seek(static_cast<std::size_t>(offset));
        const std::uint32_t next = r.u32();
        const std::uint32_t length = r.u32();
        SndSound sound;
        sound.group = r.s32();
        sound.number = r.s32();
        const std::uint64_t start = offset + kSubheaderSize;
        if (length > data.size() - start)
            return Error{"SND sound " + std::to_string(i) + " extends past the end of the file"};
        const ByteSpan bytes = data.subspan(static_cast<std::size_t>(start), length);
        sound.data.assign(bytes.begin(), bytes.end());
        doc.sounds.push_back(std::move(sound));
        if (i + 1 < count) {
            if (next <= offset)
                return Error{"SND subheader " + std::to_string(i) + " has an invalid next offset"};
            offset = next;
        }
    }
    return doc;
}

Result<SndDocument> SndDocument::load(const std::filesystem::path& path)
{
    auto data = readFile(path);
    if (!data)
        return data.error();
    return read(*data);
}

Bytes SndDocument::write() const
{
    std::size_t total = kHeaderSize;
    for (const SndSound& s : sounds)
        total += kSubheaderSize + s.data.size();
    ByteWriter out(total);
    out.text(kSignature);
    for (const std::uint8_t b : versionBytes)
        out.u8(b);
    out.u32(static_cast<std::uint32_t>(sounds.size()));
    out.u32(static_cast<std::uint32_t>(kHeaderSize));
    out.bytes(std::span<const std::uint8_t>(comment));
    for (const SndSound& s : sounds) {
        const std::size_t next = out.size() + kSubheaderSize + s.data.size();
        out.u32(static_cast<std::uint32_t>(next));
        out.u32(static_cast<std::uint32_t>(s.data.size()));
        out.s32(s.group);
        out.s32(s.number);
        out.bytes(s.data);
    }
    return out.take();
}

Status SndDocument::save(const std::filesystem::path& path) const
{
    return writeFile(path, write());
}

std::optional<std::size_t> SndDocument::find(std::int32_t group, std::int32_t number) const
{
    for (std::size_t i = 0; i < sounds.size(); ++i) {
        if (sounds[i].group == group && sounds[i].number == number)
            return i;
    }
    return std::nullopt;
}

} // namespace OFS::Formats
