#include "core/palettes/PaletteOps.h"

#include <QTest>

using namespace OFS;
using namespace OFS::Formats;
using OFS::PaletteOps::Effect;

namespace {

bool near(Rgba a, Rgba b, int tolerance = 1)
{
    return std::abs(a.r - b.r) <= tolerance && std::abs(a.g - b.g) <= tolerance && std::abs(a.b - b.b) <= tolerance &&
           a.a == b.a;
}

Image image(const Palette& palette)
{
    return Image::indexed(4, 2, std::vector<std::uint8_t>{0, 1, 2, 3, 4, 5, 6, 7}, palette);
}

std::vector<Rgba> look(const Image& image)
{
    const Image rgba = image.toRgba(true);
    std::vector<Rgba> colors;
    for (std::size_t i = 0; i + 3 < rgba.pixels().size(); i += 4)
        colors.push_back(Rgba{rgba.pixels()[i], rgba.pixels()[i + 1], rgba.pixels()[i + 2], rgba.pixels()[i + 3]});
    return colors;
}

} // namespace

class PaletteOpsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void effects()
    {
        QCOMPARE(PaletteOps::effectNames().size(), 12);
        QCOMPARE(PaletteOps::effectNames().first(), QStringLiteral("Blue to Red"));
        const Rgba c{10, 20, 30, 200};
        QCOMPARE(PaletteOps::effect(Effect::BlueToRed, c), (Rgba{30, 20, 10, 200}));
        QCOMPARE(PaletteOps::effect(Effect::GreenToBlue, c), (Rgba{10, 30, 20, 200}));
        QCOMPARE(PaletteOps::effect(Effect::RedToGreen, c), (Rgba{20, 10, 30, 200}));
        QCOMPARE(PaletteOps::effect(Effect::Lighter, Rgba{240, 0, 100, 255}), (Rgba{255, 32, 132, 255}));
        QCOMPARE(PaletteOps::effect(Effect::Darker, Rgba{240, 0, 100, 255}), (Rgba{208, 0, 68, 255}));
        // Gray g = (R+G+B)/3 = 20.
        QCOMPARE(PaletteOps::effect(Effect::Monochrome, c), (Rgba{20, 20, 20, 200}));
        QCOMPARE(PaletteOps::effect(Effect::Fire, c), (Rgba{120, 10, 0, 200}));
        QCOMPARE(PaletteOps::effect(Effect::Sapphire, c), (Rgba{65, 0, 70, 200}));
        QCOMPARE(PaletteOps::effect(Effect::Stone, c), (Rgba{70, 70, 70, 200}));
        QCOMPARE(PaletteOps::effect(Effect::Stone, Rgba{255, 255, 255, 255}), (Rgba{205, 205, 205, 255}));
        Palette palette(4, c);
        PaletteOps::applyEffect(palette, {1, 3, 9}, Effect::Monochrome);
        QCOMPARE(palette[0], c);
        QCOMPARE(palette[1], (Rgba{20, 20, 20, 200}));
        QCOMPARE(palette[3], (Rgba{20, 20, 20, 200}));
    }

    void adjustments()
    {
        Palette palette{Rgba{0, 255, 0, 255}, Rgba{0, 0, 255, 255}, Rgba{0, 0, 255, 255}};
        PaletteOps::adjustHueSaturation(palette, {0}, 120, 0);
        QVERIFY(near(palette[0], Rgba{0, 0, 255, 255}));
        PaletteOps::adjustHueSaturation(palette, {1}, 0, -50);
        QVERIFY2(near(palette[1], Rgba{26, 26, 230, 255}),
                 qPrintable(QStringLiteral("%1,%2,%3").arg(palette[1].r).arg(palette[1].g).arg(palette[1].b)));
        PaletteOps::adjustHueSaturation(palette, {2}, 0, 50);
        QVERIFY(near(palette[2], Rgba{1, 1, 255, 255}));
        Palette rgb{Rgba{100, 100, 10, 7}};
        PaletteOps::adjustRgb(rgb, {0}, 50, 200, -20);
        QCOMPARE(rgb[0], (Rgba{150, 255, 0, 7}));
    }

    void gradientAndOrder()
    {
        Palette palette(12, Rgba{0, 0, 0, 255});
        PaletteOps::gradient(palette, {11, 2, 3, 4, 5, 6, 7, 8, 9, 10}, Rgba{0, 0, 255, 255}, Rgba{255, 0, 0, 255});
        const int red[] = {0, 28, 56, 85, 113, 141, 170, 198, 226, 255};
        for (int k = 0; k < 10; ++k) {
            QCOMPARE(int(palette[std::size_t(k + 2)].r), red[k]);
            QCOMPARE(int(palette[std::size_t(k + 2)].b), 255 - red[k]);
        }
        QCOMPARE(palette[1], (Rgba{0, 0, 0, 255}));

        Palette colors{Rgba{1, 0, 0, 0}, Rgba{2, 0, 0, 255}, Rgba{3, 0, 0, 255}, Rgba{4, 0, 0, 128}};
        PaletteOps::reverseSlots(colors, {1, 3});
        QCOMPARE(colors[1].r, std::uint8_t(4));
        QCOMPARE(colors[3].r, std::uint8_t(2));
        PaletteOps::invert(colors);
        QCOMPARE(colors[0], (Rgba{254, 255, 255, 0}));
        QCOMPARE(colors[1], (Rgba{251, 255, 255, 128}));
    }

    void sortKeepsTheLook()
    {
        Palette palette{Rgba{0, 0, 0, 0},     Rgba{0, 0, 255, 255},  Rgba{255, 0, 0, 255},   Rgba{200, 200, 200, 255},
                        Rgba{0, 255, 0, 255}, Rgba{50, 50, 50, 255}, Rgba{255, 255, 0, 255}, Rgba{0, 0, 0, 0}};
        Image picture = image(palette);
        const std::vector<Rgba> before = look(picture);
        const PaletteOps::SlotMap map = PaletteOps::sortByTone(palette);
        PaletteOps::remapImage(picture, map);
        picture.setPalette(palette);
        QCOMPARE(look(picture), before);
        QCOMPARE(palette[0], (Rgba{0, 0, 0, 0}));
        QCOMPARE(palette[1], (Rgba{50, 50, 50, 255}));
        QCOMPARE(palette[2], (Rgba{200, 200, 200, 255}));
        QCOMPARE(palette[3], (Rgba{255, 0, 0, 255}));
        QCOMPARE(palette[4], (Rgba{255, 255, 0, 255}));
        QCOMPARE(palette[5], (Rgba{0, 255, 0, 255}));
        QCOMPARE(palette[6], (Rgba{0, 0, 255, 255}));
        QCOMPARE(palette[7].a, std::uint8_t(0));
    }

    void duplicates()
    {
        const Rgba blue{0, 0, 255, 255};
        const Rgba orange{255, 128, 0, 255};
        Palette palette{Rgba{0, 0, 0, 0}, Rgba{9, 9, 9, 255}, blue, orange, orange, blue, orange, Rgba{0, 0, 0, 0}};
        Image picture = image(palette);
        const std::vector<Rgba> before = look(picture);
        const PaletteOps::SlotMap map = PaletteOps::removeDuplicates(palette, Rgba{255, 0, 255, 255});
        QCOMPARE(map[2], 5);
        QCOMPARE(map[3], 6);
        QCOMPARE(map[4], 6);
        QCOMPARE(palette[2], (Rgba{255, 0, 255, 255}));
        QCOMPARE(palette[3], (Rgba{255, 1, 0, 255}));
        QCOMPARE(palette[4], (Rgba{255, 1, 1, 255}));
        QCOMPARE(palette[5], blue);
        QCOMPARE(palette[7], (Rgba{0, 0, 0, 0}));
        PaletteOps::remapImage(picture, map);
        picture.setPalette(palette);
        QCOMPARE(look(picture), before);
    }

    void addDifferent()
    {
        Palette palette{Rgba{0, 0, 0, 0}, Rgba{1, 1, 1, 255}, Rgba{0, 0, 0, 0}, Rgba{0, 0, 0, 0}};
        const Palette other{Rgba{5, 5, 5, 0}, Rgba{1, 1, 1, 255}, Rgba{7, 7, 7, 255}, Rgba{8, 8, 8, 255},
                            Rgba{9, 9, 9, 255}};
        QCOMPARE(PaletteOps::addDifferent(palette, other), 2);
        QCOMPARE(palette[2], (Rgba{7, 7, 7, 255}));
        QCOMPARE(palette[3], (Rgba{8, 8, 8, 255}));
        QCOMPARE(PaletteOps::addDifferent(palette, other), 0);
    }

    void imagesAndPalettes()
    {
        const Palette a{Rgba{255, 0, 255, 255}, Rgba{255, 0, 0, 255}, Rgba{0, 0, 255, 255}};
        const Image first = Image::indexed(3, 1, {0, 1, 2}, a);
        const Image second = Image::rgba(2, 1, {0, 255, 0, 255, 255, 0, 0, 255});
        const Palette generated = PaletteOps::paletteFromImages({first, second});
        QCOMPARE(generated.size(), std::size_t(256));
        QCOMPARE(generated[0], (Rgba{0, 0, 255, 255}));
        QCOMPARE(generated[1], (Rgba{0, 255, 0, 255}));
        QCOMPARE(generated[2], (Rgba{255, 0, 0, 255}));
        QCOMPARE(generated[3], (Rgba{255, 0, 255, 0}));
        QCOMPARE(generated[4], (Rgba{0, 0, 0, 0}));

        Palette palette(256, Rgba{0, 0, 0, 255});
        palette[0] = Rgba{0, 0, 0, 0};
        palette[40] = Rgba{1, 2, 3, 255};
        palette[90] = Rgba{4, 5, 6, 255};
        palette[91] = Rgba{1, 2, 3, 255};
        Image picture = Image::indexed(4, 1, {0, 90, 40, 91}, palette);
        const std::vector<Rgba> before = look(picture);
        PaletteOps::optimize(picture, palette);
        QCOMPARE(palette[1], (Rgba{1, 2, 3, 255}));
        QCOMPARE(palette[2], (Rgba{4, 5, 6, 255}));
        QCOMPARE(palette[3], (Rgba{0, 0, 0, 0}));
        QCOMPARE(look(picture), before);
        QCOMPARE(int(picture.pixels()[3]), 1);

        Palette colors{Rgba{0, 0, 0, 0}, Rgba{255, 0, 0, 255}, Rgba{255, 255, 255, 255}, Rgba{255, 0, 0, 255}};
        Image gray = Image::indexed(4, 1, {0, 1, 2, 3}, colors);
        PaletteOps::grayscale(gray, colors);
        QCOMPARE(colors[1], (Rgba{76, 76, 76, 255}));
        QCOMPARE(colors[2], (Rgba{255, 255, 255, 255}));
        QCOMPARE(colors[3].a, std::uint8_t(0));
        QCOMPARE(std::vector<std::uint8_t>(gray.pixels().begin(), gray.pixels().end()),
                 (std::vector<std::uint8_t>{0, 1, 2, 1}));

        const Palette target{Rgba{0, 0, 0, 0}, Rgba{250, 0, 0, 255}, Rgba{0, 0, 250, 255}};
        const Image reference = Image::rgba(3, 1, {240, 10, 0, 255, 10, 10, 200, 255, 9, 9, 9, 0});
        const Image adapted = PaletteOps::adapt(reference, target);
        QCOMPARE(std::vector<std::uint8_t>(adapted.pixels().begin(), adapted.pixels().end()),
                 (std::vector<std::uint8_t>{1, 2, 0}));
    }
};

QTEST_GUILESS_MAIN(PaletteOpsTest)
#include "PaletteOpsTest.moc"
