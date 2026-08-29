#pragma once

#include "core/CoreExport.h"
#include "formats/image/Image.h"

#include <QList>
#include <QStringList>

#include <vector>

namespace OFS::PaletteOps {

// Color functions of the Advanced palette editor. Slots outside the palette are ignored.

enum class Effect
{
    BlueToRed,
    BlueToGreen,
    GreenToRed,
    GreenToBlue,
    RedToGreen,
    RedToBlue,
    Lighter,
    Darker,
    Monochrome,
    Fire,
    Sapphire,
    Stone,
};
OFS_CORE_EXPORT QStringList effectNames();
// Alpha is kept. Channel swaps, +/-32 per channel, gray g = (R+G+B)/3, Fire (g+100, g/2, 0),
// Sapphire (60+g/4, 0, 60+g/2), Stone min(g+50, 205).
OFS_CORE_EXPORT Formats::Rgba effect(Effect effect, Formats::Rgba color);
OFS_CORE_EXPORT void applyEffect(Formats::Palette& palette, const QList<int>& slots, Effect effect);

// Turns the hue by `hue` degrees and moves the HSL saturation by `saturation`/255, clamped.
OFS_CORE_EXPORT void adjustHueSaturation(Formats::Palette& palette, const QList<int>& slots, int hue, int saturation);
OFS_CORE_EXPORT void adjustRgb(Formats::Palette& palette, const QList<int>& slots, int red, int green, int blue);

// Slot k of n (in index order) gets start + (end - start) * k / (n - 1), truncated.
OFS_CORE_EXPORT void gradient(Formats::Palette& palette, QList<int> slots, Formats::Rgba start, Formats::Rgba end);
OFS_CORE_EXPORT void reverseSlots(Formats::Palette& palette, QList<int> slots);
// Inverts every RGB value and keeps alpha. Fighter Factory reversed the slot order instead.
OFS_CORE_EXPORT void invert(Formats::Palette& palette);

// Old index -> new index, for remapping an image.
using SlotMap = std::vector<int>;
OFS_CORE_EXPORT void remapImage(Formats::Image& image, const SlotMap& map);

OFS_CORE_EXPORT SlotMap sortByTone(Formats::Palette& palette);
// Every copy but the last gets the first unused color from `base` on (Fighter Factory gave them all
// `base`).
OFS_CORE_EXPORT SlotMap removeDuplicates(Formats::Palette& palette, Formats::Rgba base);
OFS_CORE_EXPORT int addDifferent(Formats::Palette& palette, const Formats::Palette& other);

OFS_CORE_EXPORT std::vector<Formats::Rgba> usedColors(const Formats::Image& image);
OFS_CORE_EXPORT Formats::Palette paletteFromImages(const QList<Formats::Image>& images);
// Keeps only the colors the image uses and re-indexes it (Fighter Factory stopped drawing it).
OFS_CORE_EXPORT void optimize(Formats::Image& image, Formats::Palette& palette);
OFS_CORE_EXPORT void grayscale(Formats::Image& image, Formats::Palette& palette);
OFS_CORE_EXPORT Formats::Image adapt(const Formats::Image& reference, const Formats::Palette& palette);

} // namespace OFS::PaletteOps
