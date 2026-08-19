#include "formats/image/Quantize.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace OFS::Formats {

namespace {

struct ColorCount
{
    std::array<std::uint8_t, 3> rgb;
    std::uint32_t count;
};

std::uint32_t key(std::uint8_t r, std::uint8_t g, std::uint8_t b)
{
    return (std::uint32_t(r) << 16) | (std::uint32_t(g) << 8) | b;
}

struct Box
{
    std::size_t begin;
    std::size_t end;
    int axis = 0; // the channel with the widest range
    int range = 0;
    std::uint64_t weight = 0;
};

void measure(Box& box, const std::vector<ColorCount>& colors)
{
    std::array<int, 3> low{255, 255, 255};
    std::array<int, 3> high{0, 0, 0};
    box.weight = 0;
    for (std::size_t i = box.begin; i < box.end; ++i) {
        for (int c = 0; c < 3; ++c) {
            low[std::size_t(c)] = std::min<int>(low[std::size_t(c)], colors[i].rgb[std::size_t(c)]);
            high[std::size_t(c)] = std::max<int>(high[std::size_t(c)], colors[i].rgb[std::size_t(c)]);
        }
        box.weight += colors[i].count;
    }
    box.axis = 0;
    box.range = -1;
    for (int c = 0; c < 3; ++c) {
        if (high[std::size_t(c)] - low[std::size_t(c)] > box.range) {
            box.range = high[std::size_t(c)] - low[std::size_t(c)];
            box.axis = c;
        }
    }
}

Palette medianCut(std::vector<ColorCount> colors, std::size_t count)
{
    std::vector<Box> boxes{Box{0, colors.size()}};
    measure(boxes[0], colors);
    while (boxes.size() < count) {
        // Split the box with the largest pixel-weighted range at its median.
        std::size_t pick = boxes.size();
        double best = 0;
        for (std::size_t i = 0; i < boxes.size(); ++i) {
            const double score = double(boxes[i].range) * double(boxes[i].weight);
            if (boxes[i].end - boxes[i].begin > 1 && score > best) {
                best = score;
                pick = i;
            }
        }
        if (pick == boxes.size())
            break;
        Box box = boxes[pick];
        const int axis = box.axis;
        std::sort(colors.begin() + std::ptrdiff_t(box.begin), colors.begin() + std::ptrdiff_t(box.end),
                  [axis](const ColorCount& a, const ColorCount& b) {
                      return a.rgb[std::size_t(axis)] < b.rgb[std::size_t(axis)];
                  });
        std::uint64_t half = box.weight / 2;
        std::uint64_t seen = 0;
        std::size_t middle = box.begin;
        while (middle < box.end - 1 && seen + colors[middle].count <= half)
            seen += colors[middle++].count;
        if (middle == box.begin)
            ++middle;
        Box first{box.begin, middle};
        Box second{middle, box.end};
        measure(first, colors);
        measure(second, colors);
        boxes[pick] = first;
        boxes.push_back(second);
    }
    Palette palette;
    for (const Box& box : boxes) {
        std::array<std::uint64_t, 3> sum{};
        std::uint64_t weight = 0;
        for (std::size_t i = box.begin; i < box.end; ++i) {
            for (int c = 0; c < 3; ++c)
                sum[std::size_t(c)] += std::uint64_t(colors[i].rgb[std::size_t(c)]) * colors[i].count;
            weight += colors[i].count;
        }
        if (weight == 0)
            continue;
        palette.push_back(
            {std::uint8_t(sum[0] / weight), std::uint8_t(sum[1] / weight), std::uint8_t(sum[2] / weight), 255});
    }
    return palette;
}

} // namespace

Image quantize(const Image& image)
{
    if (image.isIndexed())
        return image;
    const int width = image.width();
    const int height = image.height();
    const std::span<const std::uint8_t> in = image.pixels();
    const std::size_t pixelCount = std::size_t(width) * std::size_t(height);

    std::unordered_map<std::uint32_t, std::uint32_t> histogram;
    for (std::size_t i = 0; i < pixelCount; ++i) {
        if (in[i * 4 + 3] >= 128)
            ++histogram[key(in[i * 4], in[i * 4 + 1], in[i * 4 + 2])];
    }
    std::vector<ColorCount> colors;
    colors.reserve(histogram.size());
    for (const auto& [k, n] : histogram)
        colors.push_back({{std::uint8_t(k >> 16), std::uint8_t(k >> 8), std::uint8_t(k)}, n});
    std::sort(colors.begin(), colors.end(), [](const ColorCount& a, const ColorCount& b) { return a.count > b.count; });

    Palette visible;
    if (colors.size() <= 255) {
        for (const ColorCount& c : colors)
            visible.push_back({c.rgb[0], c.rgb[1], c.rgb[2], 255});
    } else {
        visible = medianCut(colors, 255);
    }
    Palette palette(visible.size() <= 31 ? 32 : 256, Rgba{0, 0, 0, 255});
    std::copy(visible.begin(), visible.end(), palette.begin() + 1);

    std::unordered_map<std::uint32_t, std::uint8_t> lookup;
    std::vector<std::uint8_t> indices(pixelCount, 0);
    for (std::size_t i = 0; i < pixelCount; ++i) {
        if (in[i * 4 + 3] < 128)
            continue;
        const std::uint32_t k = key(in[i * 4], in[i * 4 + 1], in[i * 4 + 2]);
        auto found = lookup.find(k);
        if (found == lookup.end()) {
            int best = 1;
            int bestDistance = INT32_MAX;
            for (std::size_t p = 0; p < visible.size(); ++p) {
                const int dr = int(visible[p].r) - in[i * 4];
                const int dg = int(visible[p].g) - in[i * 4 + 1];
                const int db = int(visible[p].b) - in[i * 4 + 2];
                const int distance = dr * dr + dg * dg + db * db;
                if (distance < bestDistance) {
                    bestDistance = distance;
                    best = int(p) + 1;
                }
            }
            found = lookup.emplace(k, std::uint8_t(best)).first;
        }
        indices[i] = found->second;
    }
    return Image::indexed(width, height, std::move(indices), std::move(palette));
}

} // namespace OFS::Formats
