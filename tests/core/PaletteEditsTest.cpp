#include "core/sprites/PaletteEdits.h"
#include "core/sprites/SpriteFile.h"
#include "formats/palette/PaletteFormats.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QUndoStack>

using namespace OFS;
using namespace OFS::Formats;

namespace {

// 256 colors: entry i = (i, 255 - i, 7i mod 256), slot 0 transparent.
Palette ramp(std::uint8_t shift = 0)
{
    Palette colors(256);
    for (int i = 0; i < 256; ++i)
        colors[std::size_t(i)] = Rgba{std::uint8_t(i + shift), std::uint8_t(255 - i), std::uint8_t(7 * i), 255};
    colors[0].a = 0;
    return colors;
}

// Palettes 1,1 / 1,2 / 600,1 (same colors as 1,1) / 600,2. Sprites 0,0 and 0,1 use 1,1, 5,0 uses
// 600,1.
SpriteDocument document()
{
    SpriteDocument doc;
    doc.palettes() << SpriteDocument::makePalette(1, 1, ramp()) << SpriteDocument::makePalette(1, 2, ramp(1))
                   << SpriteDocument::makePalette(600, 1, ramp()) << SpriteDocument::makePalette(600, 2, ramp(2));
    const int ids[][3] = {{0, 0, 0}, {0, 1, 0}, {5, 0, 2}};
    for (const auto& id : ids) {
        SpriteEntry entry;
        entry.header.group = std::uint16_t(id[0]);
        entry.header.number = std::uint16_t(id[1]);
        entry.pixels = SpriteDocument::makePixels(Image::indexed(2, 2, std::vector<std::uint8_t>{0, 1, 2, 3}));
        entry.palette = doc.palettes()[id[2]];
        doc.sprites() << entry;
    }
    return doc;
}

QByteArray readAll(const QString& path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

} // namespace

class PaletteEditsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void missingPalette()
    {
        SpriteDocument doc = document();
        QVERIFY(PaletteEdits::spritesWithoutPalette(doc).isEmpty());
        QCOMPARE(PaletteEdits::addMissingPalette(doc), -1);
        PaletteEdits::clear(doc);
        const QList<int> missing = PaletteEdits::spritesWithoutPalette(doc);
        QCOMPARE(missing.size(), doc.spriteCount());
        const int index = PaletteEdits::addMissingPalette(doc);
        QCOMPARE(index, 0);
        QCOMPARE(doc.palettes()[0]->palette.group, std::uint16_t(1));
        QCOMPARE(doc.palettes()[0]->palette.number, std::uint16_t(1));
        QCOMPARE(doc.palettes()[0]->palette.colors[200], (Rgba{200, 200, 200, 255}));
        QVERIFY(PaletteEdits::spritesWithoutPalette(doc).isEmpty());
        QCOMPARE(PaletteEdits::issues(doc).size(), 0);
    }

    void colorEdits()
    {
        SpriteDocument doc = document();
        const SpritePalettePtr before = doc.palettes()[0];
        PaletteEdits::fill(doc, 0, {1, 2}, Rgba{200, 100, 0, 128});
        QVERIFY(doc.palettes()[0] != before);
        QCOMPARE(doc.sprites()[0].palette, doc.palettes()[0]);
        QCOMPARE(doc.sprites()[1].palette, doc.palettes()[0]);
        QCOMPARE(doc.palettes()[0]->palette.colors[2], (Rgba{200, 100, 0, 128}));
        QCOMPARE(before->palette.colors[2], ramp()[2]);

        doc = document();
        PaletteEdits::exchange(doc, 0, {6, 2, 3});
        QCOMPARE(doc.palettes()[0]->palette.colors[2], ramp()[6]);
        QCOMPARE(doc.palettes()[0]->palette.colors[3], ramp()[3]);
        QCOMPARE(doc.palettes()[0]->palette.colors[6], ramp()[2]);

        doc = document();
        PaletteEdits::moveColor(doc, 0, 5, 9);
        QCOMPARE(doc.palettes()[0]->palette.colors[5], ramp()[6]);
        QCOMPARE(doc.palettes()[0]->palette.colors[8], ramp()[9]);
        QCOMPARE(doc.palettes()[0]->palette.colors[9], ramp()[5]);
        PaletteEdits::moveColor(doc, 0, 9, 5);
        QCOMPARE(doc.palettes()[0]->palette.colors, ramp());

        doc = document();
        PaletteEdits::switchCount(doc, 1);
        QCOMPARE(doc.palettes()[1]->palette.colors.size(), std::size_t(32));
        QCOMPARE(doc.sprites()[0].palette, doc.palettes()[0]);
        PaletteEdits::switchCount(doc, 1);
        QCOMPARE(doc.palettes()[1]->palette.colors, ramp(1));
        PaletteEdits::setIdentifiers(doc, 1, 9, 9);
        QCOMPARE(doc.findPalette(9, 9), 1);
    }

    void listEdits()
    {
        SpriteDocument doc = document();
        QCOMPARE(PaletteEdits::insert(doc, 0, 0, 0, PaletteEdits::emptyColors()), 1);
        QCOMPARE(doc.palettes()[1]->palette.colors.size(), std::size_t(32));
        QCOMPARE(doc.palettes()[1]->palette.colors[31], (Rgba{0, 0, 0, 0}));
        QCOMPARE(PaletteEdits::insert(doc, -1, 7, 7, PaletteEdits::emptyColors()), 0);

        doc = document();
        QCOMPARE(PaletteEdits::users(doc, 0), (QList<int>{0, 1}));
        PaletteEdits::remove(doc, 0, 1);
        QCOMPARE(doc.palettes().size(), 3);
        QCOMPARE(doc.sprites()[0].palette, doc.palettes()[0]);
        QCOMPARE(doc.palettes()[0]->palette.number, std::uint16_t(2));
        PaletteEdits::remove(doc, 0, -1);
        QVERIFY(!doc.sprites()[0].palette);
        QCOMPARE(PaletteEdits::issues(doc).size(), 2);

        doc = document();
        QCOMPARE(PaletteEdits::removeDuplicates(doc), 1);
        QCOMPARE(doc.palettes().size(), 3);
        QCOMPARE(doc.findPalette(600, 1), -1);
        QCOMPARE(doc.sprites()[2].palette, doc.palettes()[0]);

        doc = document();
        doc.palettes() << SpriteDocument::makePalette(600, 3, ramp(3)) << SpriteDocument::makePalette(600, 4, ramp(4));
        QCOMPARE(PaletteEdits::removeUnused(doc), 3);
        QCOMPARE(doc.palettes().size(), 3);
        QCOMPARE(doc.findPalette(1, 2), 1);
        QCOMPARE(doc.findPalette(600, 1), 2);

        PaletteEdits::clear(doc);
        QVERIFY(doc.palettes().isEmpty());
        QVERIFY(!doc.sprites()[2].palette);

        doc = document();
        PaletteEdits::applyToSprites(doc, 3, {0, 2});
        QCOMPARE(doc.sprites()[0].palette, doc.palettes()[3]);
        QCOMPARE(doc.sprites()[1].palette, doc.palettes()[0]);
        QCOMPARE(PaletteEdits::paletteOf(doc, 2), 3);
    }

    void issues()
    {
        SpriteDocument doc = document();
        QVERIFY(PaletteEdits::issues(doc).isEmpty());
        PaletteEdits::setIdentifiers(doc, 3, 1, 1);
        const IssueList issues = PaletteEdits::issues(doc);
        QCOMPARE(issues.size(), 1);
        QCOMPARE(issues.first().message, QStringLiteral("The group 1, index 1 is already in use by palette #1!"));
        QCOMPARE(issues.first().line, 3);
    }

    void files()
    {
        QTemporaryDir dir;
        // ACT files are stored in reverse: file entry i is slot 255 - i.
        QByteArray act;
        for (int i = 0; i < 256; ++i) {
            act += char(i);
            act += char(255 - i);
            act += char(7 * i);
        }
        QFile out(dir.filePath("test.act"));
        QVERIFY(out.open(QIODevice::WriteOnly));
        out.write(act);
        out.close();
        auto colors = PaletteEdits::readFile(dir.filePath("test.act"));
        QVERIFY(colors.ok());
        QCOMPARE(colors.value().size(), std::size_t(256));
        QCOMPARE(colors.value()[255], (Rgba{0, 255, 0, 255}));
        QCOMPARE(colors.value()[1], (Rgba{254, 1, std::uint8_t(7 * 254), 255}));
        QCOMPARE(colors.value()[0].a, std::uint8_t(0));
        colors = PaletteEdits::readFile(dir.filePath("test.act"), true);
        QCOMPARE(colors.value()[1], (Rgba{1, 254, 7, 255}));

        QFile jasc(dir.filePath("four.pal"));
        QVERIFY(jasc.open(QIODevice::WriteOnly));
        jasc.write("JASC-PAL\r\n0100\r\n4\r\n255 0 255\r\n255 0 0\r\n0 255 0\r\n0 0 255\r\n");
        jasc.close();
        colors = PaletteEdits::readFile(dir.filePath("four.pal"));
        QVERIFY(colors.ok());
        QCOMPARE(colors.value().size(), std::size_t(32));
        QCOMPARE(colors.value()[1], (Rgba{255, 0, 0, 255}));
        QCOMPARE(colors.value()[0], (Rgba{255, 0, 255, 0}));

        QFile painter(dir.filePath("set.txt"));
        QVERIFY(painter.open(QIODevice::WriteOnly));
        painter.write("ROWS 1\nCOLS 2\nWIDTH 16\nHEIGHT 16\nTEXTHEIGHT 0\nSPACING 1\n"
                      "R: 10, G:20, B: 30 HV: 0.58, SV: 0.67, VV: 0.12 \"Dark\"\nR:255, G:255, B:255\n");
        painter.close();
        colors = PaletteEdits::readFile(dir.filePath("set.txt"));
        QVERIFY2(colors.ok(), colors ? "" : colors.error().message.c_str());
        QCOMPARE(colors.value()[0], (Rgba{10, 20, 30, 0}));
        QCOMPARE(colors.value()[1], (Rgba{255, 255, 255, 255}));

        const SpriteDocument doc = document();
        QCOMPARE(PaletteEdits::saveAct(doc, {0}, dir.filePath("one")), QStringList{dir.filePath("one.act")});
        const QByteArray saved = readAll(dir.filePath("one.act"));
        QCOMPARE(saved.size(), 768);
        QCOMPARE(saved.left(3), QByteArray("\xff\x00\xf9", 3));
        QCOMPARE(PaletteEdits::saveAct(doc, {0, 3}, dir.filePath("multi.act")),
                 (QStringList{dir.filePath("multi_1-1.act"), dir.filePath("multi_600-2.act")}));
    }

    void colorUndo()
    {
        QTemporaryDir dir;
        SpriteFile file(dir.filePath("x.sff"), "sprites");
        file.createEmpty(SffVersion::V2);
        SpriteDocument doc = document();
        file.apply("Add", doc);
        PaletteEdits::fill(doc, 0, {1}, Rgba{1, 2, 3, 255});
        file.applyColorEdit("Color", doc);
        PaletteEdits::exchange(doc, 0, {1, 2});
        file.applyColorEdit("Exchange", doc);
        QVERIFY(file.undoColorEdit());
        QCOMPARE(file.document().palettes()[0]->palette.colors[1], (Rgba{1, 2, 3, 255}));
        QVERIFY(file.undoColorEdit());
        QCOMPARE(file.document().palettes()[0]->palette.colors[1], ramp()[1]);
        QVERIFY(!file.undoColorEdit());
        QCOMPARE(file.undoStack()->index(), 1);
        QVERIFY(file.redoColorEdit());
        QVERIFY(file.redoColorEdit());
        QVERIFY(!file.redoColorEdit());
        PaletteEdits::switchCount(doc, 0);
        file.apply("Switch", doc);
        QVERIFY(!file.undoColorEdit());
    }
};

QTEST_GUILESS_MAIN(PaletteEditsTest)
#include "PaletteEditsTest.moc"
