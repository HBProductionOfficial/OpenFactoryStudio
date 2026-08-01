#include "formats/maker/SndMakerScript.h"

#include "formats/maker/MakerText.h"

#include <set>

namespace OFS::Formats {

using namespace Maker;

Result<SndMakerScript> SndMakerScript::parse(std::string_view text)
{
    const std::vector<std::string_view> all = lines(text);
    SndMakerScript script;
    if (all.empty() || all[0].empty())
        return Error{"the SND file name is missing"};
    script.outputFile = std::string(all[0]);
    std::size_t index = 1;
    while (index < all.size() && !all[index].empty()) {
        SndMakerSound sound;
        sound.file = std::string(all[index]);
        if (index + 2 >= all.size())
            return Error{"line " + std::to_string(index + 1) + ": the group and sound numbers of " + sound.file +
                         " are missing"};
        if (!parseInt(all[index + 1], sound.group))
            return Error{"line " + std::to_string(index + 2) + ": invalid group number"};
        if (!parseInt(all[index + 2], sound.number))
            return Error{"line " + std::to_string(index + 3) + ": invalid sound number"};
        script.sounds.push_back(std::move(sound));
        index += 3;
    }
    return script;
}

Result<std::string> SndMakerScript::write() const
{
    auto valid = [](const std::string& name) {
        return !name.empty() && name.find_first_of("\r\n") == std::string::npos;
    };
    if (!valid(outputFile))
        return Error{"invalid SND file name '" + outputFile + "'"};
    std::string out = outputFile + "\r\n";
    for (const SndMakerSound& sound : sounds) {
        if (!valid(sound.file))
            return Error{"invalid sound file name '" + sound.file + "'"};
        out += sound.file + "\r\n" + std::to_string(sound.group) + "\r\n" + std::to_string(sound.number) + "\r\n";
    }
    out += "\r\n";
    return out;
}

Result<SndMakerExport> exportSndMaker(const SndDocument& snd, const SndMakerExportOptions& options)
{
    SndMakerExport result;
    result.script.outputFile = options.outputFile;
    std::vector<std::size_t> order = options.sounds;
    if (order.empty()) {
        order.resize(snd.sounds.size());
        for (std::size_t i = 0; i < order.size(); ++i)
            order[i] = i;
    }
    std::set<std::string> names;
    for (const std::size_t index : order) {
        if (index >= snd.sounds.size())
            return Error{"sound index " + std::to_string(index) + " is out of range"};
        const SndSound& sound = snd.sounds[index];
        const std::string stem = std::to_string(sound.group) + "_" + std::to_string(sound.number);
        std::string name = stem + ".wav";
        for (int copy = 2; !names.insert(name).second; ++copy)
            name = stem + "_" + std::to_string(copy) + ".wav";
        result.files.push_back({name, sound.data});
        result.script.sounds.push_back({options.pathPrefix + name, sound.group, sound.number});
    }
    return result;
}

} // namespace OFS::Formats
