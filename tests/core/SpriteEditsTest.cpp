#include "core/sprites/SpriteEdits.h"
#include "core/sprites/SpriteDocument.h"
#include "core/sprites/SpriteExport.h"
#include "core/sprites/SpriteFile.h"
#include "core/sprites/SpriteImages.h"
#include "fixtures/FixtureFactory.h"
#include "formats/codecs/Png.h"

#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUndoStack>

using namespace OFS;
using namespace OFS::Formats;

namespace {

// A small document: sprite 0,0 (5x4 with a transparent border), 0,1 linked to it, 1,0 (3x3).
SpriteDocument sample()
{
    Palette colors = grayscalePalette(256);
    colors[1] = {255, 0, 0, 255};
    colors[2] = {0, 255, 0, 255};
    colors[3] = {0, 0, 255, 255};
    std::vector<std::uint8_t> bordered = {0, 0, 0, 0, 0, //
                                          0, 1, 2, 0, 0, //
                                          0, 3, 1, 0, 0, //
                                          0, 0, 0, 0, 0};
    SpriteDocument doc;
    const SpritePalettePtr palette = SpriteDocument::makePalette(1, 1, colors);
    doc.palettes() << palette;
    SpriteEntry a;
    a.header.axisX = 2;
    a.header.axisY = 3;
    a.pixels = SpriteDocument::makePixels(Image::indexed(5, 4, bordered));
    a.palette = palette;
    SpriteEntry b = a;
    b.header.number = 1;
    SpriteEntry c;
    c.header.group = 1;
    c.pixels = SpriteDocument::makePixels(Image::indexed(3, 3, std::vector<std::uint8_t>(9, 2)));
    c.palette = palette;
    doc.sprites() << a << b << c;
    return doc;
}

void writeFile(const QString& path, const QByteArray& data)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

QByteArray toBytes(const Bytes& bytes)
{
    return QByteArray(reinterpret_cast<const char*>(bytes.data()), qsizetype(bytes.size()));
}

} // namespace

class SpriteEditsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void identifiersAndAxes()
    {
        SpriteDocument doc = sample();
        SpriteEdits::setIdentifiers(doc, 2, 70000, -5);
        QCOMPARE(doc.sprites()[2].group(), 0xFFFF);
        QCOMPARE(doc.sprites()[2].number(), 0);
        SpriteEdits::setAxis(doc, 0, 40000, -40000);
        QCOMPARE(int(doc.sprites()[0].header.axisX), 32767);
        QCOMPARE(int(doc.sprites()[0].header.axisY), -32768);
        SpriteEdits::setAxis(doc, 0, 2, 3);
        SpriteEdits::moveAxes(doc, {0, 2, 0, 9}, 1, -2);
        QCOMPARE(int(doc.sprites()[0].header.axisX), 3);
        QCOMPARE(int(doc.sprites()[0].header.axisY), 1);
        QCOMPARE(int(doc.sprites()[1].header.axisX), 2);
        QCOMPARE(int(doc.sprites()[2].header.axisY), -2);
    }

    void groups()
    {
        SpriteDocument doc = sample();
        QCOMPARE(SpriteEdits::renameGroup(doc, 0, 5), 2);
        QCOMPARE(doc.findSprite(5, 1), 1);
        QCOMPARE(SpriteEdits::removeGroup(doc, 5), 2);
        QCOMPARE(doc.spriteCount(), 1);
        QCOMPARE(doc.palettes().size(), 1);
        doc = sample();
        SpriteEdits::remove(doc, {2, 0, 2, 7});
        QCOMPARE(doc.spriteCount(), 1);
        QCOMPARE(doc.sprites()[0].number(), 1);
    }

    void groupIntervals()
    {
        SpriteDocument doc = sample();
        for (int n = 2; n <= 6; ++n) {
            SpriteEntry entry = doc.sprites()[0];
            entry.header.number = std::uint16_t(n);
            doc.sprites() << entry;
        }
        QCOMPARE(SpriteEdits::removeGroup(doc, 0, 3, 5), 3);
        QCOMPARE(doc.findSprite(0, 4), -1);
        QVERIFY(doc.findSprite(0, 6) >= 0);
        QCOMPARE(SpriteEdits::renameGroup(doc, 0, 7, 1, 2), 2);
        QVERIFY(doc.findSprite(7, 1) >= 0);
        QVERIFY(doc.findSprite(0, 0) >= 0);
    }

    void imageExport()
    {
        QTemporaryDir dir;
        SpriteDocument doc = sample();
        QString error;
        QStringList written = saveSpriteImages(doc, {2}, dir.filePath("one"), "png", &error);
        QCOMPARE(written, QStringList{dir.filePath("one.png")});
        QVERIFY(loadImageFile(written[0]).isIndexed());
        written = saveSpriteImages(doc, {0, 2}, dir.filePath("multi.pcx"), "pcx", &error);
        QCOMPARE(written, (QStringList{dir.filePath("multi_0-0.pcx"), dir.filePath("multi_1-0.pcx")}));
        QCOMPARE(loadImageFile(written[1]).width(), 3);
        QVERIFY(saveSpriteImages(doc, {1}, dir.filePath("g.gif"), "gif", &error).size() == 1);
        QVERIFY(saveSpriteImages(doc, {1}, dir.filePath("b.bmp"), "bmp", &error).size() == 1);

        // Group 11 sizes from Fighter Factory.
        SpriteDocument group;
        group.palettes() << doc.palettes()[0];
        const int sizes[][4] = {{49, 92, 17, 91}, {49, 80, 17, 79}, {50, 75, 18, 73}};
        for (const auto& size : sizes) {
            SpriteEntry entry;
            entry.header.group = 11;
            entry.header.number = std::uint16_t(group.spriteCount());
            entry.header.axisX = std::int16_t(size[2]);
            entry.header.axisY = std::int16_t(size[3]);
            entry.pixels = SpriteDocument::makePixels(Image::indexed(
                size[0], size[1], std::vector<std::uint8_t>(std::size_t(size[0]) * std::size_t(size[1]), 1)));
            entry.palette = group.palettes()[0];
            group.sprites() << entry;
        }
        const AlignedImages aligned = alignSprites(group, {0, 1, 2});
        QCOMPARE(aligned.axis, QPoint(18, 91));
        QCOMPARE(aligned.images.size(), 3);
        for (const Image& image : aligned.images) {
            QCOMPARE(image.width(), 50);
            QCOMPARE(image.height(), 93);
        }
        QCOMPARE(aligned.images[0].indexAt(0, 0), std::uint8_t(0));
        QCOMPARE(aligned.images[0].indexAt(1, 0), std::uint8_t(1));
        QCOMPARE(axisFileText(aligned.axis), QByteArray("X axis: 18\r\nY axis: 91"));
        written = saveAlignedImages(group, {0, 1, 2}, aligned, dir.filePath("al.png"), "png", &error);
        QCOMPARE(written.size(), 3);
        QCOMPARE(QFileInfo(written[2]).fileName(), QStringLiteral("al_11-2.png"));
    }

    void duplicates()
    {
        SpriteDocument doc = sample();
        QCOMPARE(SpriteEdits::freeNumber(doc, 0), 2);
        QCOMPARE(SpriteEdits::freeNumber(doc, 1, 0), 1);
        const int copy = SpriteEdits::duplicate(doc, 0);
        QCOMPARE(copy, 1);
        QCOMPARE(doc.sprites()[1].number(), 0);
        QCOMPARE(doc.linkedSprites(0), (QList<int>{0, 1, 2}));
        const SffDocument sff = doc.toSff();
        QCOMPARE(sff.sprites[1].linkedIndex, 0);
    }

    void images()
    {
        SpriteDocument doc = sample();
        Image image = Image::indexed(2, 2, {1, 1, 1, 1});
        image.setPalette(doc.palettes()[0]->palette.colors);
        SpriteEdits::setImage(doc, 1, image);
        QCOMPARE(doc.palettes().size(), 1);
        QCOMPARE(doc.linkedSprites(0), (QList<int>{0}));
        QCOMPARE(doc.sprites()[1].width(), 2);
        Image other = Image::indexed(1, 1, {5});
        other.setPalette(grayscalePalette(256));
        SpriteEdits::setImage(doc, 2, other);
        QCOMPARE(doc.palettes().size(), 2);
        QCOMPARE(int(doc.palettes()[1]->palette.group), 1);
        SpriteEdits::setImage(doc, 0, other);
        QCOMPARE(doc.palettes().size(), 2);
        QCOMPARE(doc.sprites()[0].palette, doc.sprites()[2].palette);
    }

    void cropping()
    {
        SpriteDocument doc = sample();
        SpriteEdits::crop(doc, {0, 1, 2});
        QCOMPARE(doc.sprites()[0].width(), 2);
        QCOMPARE(doc.sprites()[0].height(), 2);
        QCOMPARE(int(doc.sprites()[0].header.axisX), 1);
        QCOMPARE(int(doc.sprites()[0].header.axisY), 2);
        QCOMPARE(doc.linkedSprites(0), (QList<int>{0, 1}));
        QCOMPARE(doc.sprites()[2].width(), 3);
        QCOMPARE(doc.image(0).indexAt(1, 1), std::uint8_t(1));

        doc = sample();
        SpriteEdits::crop(doc, {1});
        QCOMPARE(doc.sprites()[0].width(), 5);
        QCOMPARE(doc.sprites()[1].width(), 2);
    }

    void resizing()
    {
        SpriteDocument doc = sample();
        SpriteEdits::resize(doc, {2}, 2.0, 3.0, ResizeFilter::Nearest);
        QCOMPARE(doc.sprites()[2].width(), 6);
        QCOMPARE(doc.sprites()[2].height(), 9);
        QVERIFY(doc.image(2).isIndexed());
        SpriteEdits::resize(doc, {0}, 2.0, 2.0, ResizeFilter::Nearest);
        QCOMPARE(int(doc.sprites()[0].header.axisX), 4);
        QCOMPARE(int(doc.sprites()[0].header.axisY), 6);
        QCOMPARE(int(doc.sprites()[1].header.axisX), 2);
    }

    void decompressOnLoad()
    {
        SpriteDocument doc = sample();
        QVERIFY(!SpriteEdits::decompressesOnLoad(doc, 1));
        SpriteEdits::setDecompressOnLoad(doc, 1, true);
        QVERIFY(SpriteEdits::decompressesOnLoad(doc, 0));
        QVERIFY(!SpriteEdits::decompressesOnLoad(doc, 2));
        QCOMPARE(doc.linkedSprites(0), (QList<int>{0, 1}));
    }

    void channels()
    {
        SpriteDocument doc = sample();
        const int blue = SpriteEdits::splitChannels(doc, 2);
        QCOMPARE(blue, 4);
        QCOMPARE(doc.spriteCount(), 5);
        QCOMPARE(doc.sprites()[2].number(), 0);
        QCOMPARE(doc.sprites()[3].number(), 1);
        QCOMPARE(doc.sprites()[4].number(), 2);
        QCOMPARE(doc.image(2).indexAt(0, 0), std::uint8_t(0));
        QCOMPARE(doc.image(3).indexAt(0, 0), std::uint8_t(255));
        QCOMPARE(doc.image(4).indexAt(0, 0), std::uint8_t(0));
        QCOMPARE(doc.linkedSprites(3), QList<int>{3});
        QCOMPARE(doc.sprites()[2].name, QStringLiteral(" R"));
        QCOMPARE(doc.sprites()[3].name, QStringLiteral(" G"));
        QCOMPARE(doc.sprites()[4].name, QStringLiteral(" B"));
        QCOMPARE(doc.palettes().size(), 4);
        QCOMPARE(int(doc.sprites()[3].palette->palette.number), 1);
        QCOMPARE(doc.sprites()[3].palette->palette.colors[200].g, std::uint8_t(200));
        QCOMPARE(doc.sprites()[3].palette->palette.colors[200].r, std::uint8_t(0));
        SpriteEdits::splitChannels(doc, 0);
        QCOMPARE(doc.palettes().size(), 4);
        QCOMPARE(doc.sprites()[1].palette, doc.sprites()[5].palette);
    }

    void namesAndTransparency()
    {
        // SFF v1 keeps names in the comment bytes.
        SpriteDocument doc = sample();
        doc.sprites()[0].name = "abcdefghijklmnop";
        SffDocument sff = doc.toSff();
        QVERIFY(sff.convertTo(SffVersion::V1).ok());
        const auto bytes = sff.write();
        QVERIFY(bytes.ok());
        auto reread = SffDocument::read(ByteSpan(bytes.value()));
        QVERIFY(reread.ok());
        const SpriteDocument back = SpriteDocument::fromSff(reread.value());
        QCOMPARE(back.sprites()[0].name, QStringLiteral("abcdefghijklm"));
        QCOMPARE(back.sprites()[1].name, QString());

        SpriteEntry rgba;
        rgba.pixels = SpriteDocument::makePixels(Image::rgba(1, 1, {10, 20, 30, 0}));
        doc.sprites() << rgba;
        QCOMPARE(doc.imageRgba(3).colorAt(0, 0).a, std::uint8_t(0));
        QCOMPARE(doc.imageRgba(3, nullptr, true).colorAt(0, 0).a, std::uint8_t(255));
        QCOMPARE(doc.imageRgba(0, nullptr, true).colorAt(0, 0).a, std::uint8_t(255));
    }

    void newSpriteCompression()
    {
        // A few colors are stored LZ5, more RLE8, opaque true color PNG24, with alpha PNG32.
        SpriteDocument doc;
        doc.palettes() << SpriteDocument::makePalette(1, 1, grayscalePalette(256));
        auto add = [&doc](Image image) {
            SpriteEntry entry;
            entry.header.number = std::uint16_t(doc.spriteCount());
            entry.pixels = SpriteDocument::makePixels(std::move(image));
            entry.palette = doc.palettes()[0];
            doc.sprites() << entry;
        };
        add(Image::indexed(2, 1, {1, 3}));
        add(Image::indexed(2, 1, {1, 200}));
        add(Image::rgba(1, 1, {1, 2, 3, 255}));
        add(Image::rgba(1, 1, {1, 2, 3, 100}));
        const auto bytes = doc.toSff().write();
        QVERIFY(bytes.ok());
        auto sff = SffDocument::read(ByteSpan(bytes.value()));
        QVERIFY(sff.ok());
        QCOMPARE(sff.value().sprites[0].compression, SffCompression::Lz5);
        QCOMPARE(sff.value().sprites[1].compression, SffCompression::Rle8);
        QCOMPARE(sff.value().sprites[2].compression, SffCompression::Png24);
        QCOMPARE(sff.value().sprites[3].compression, SffCompression::Png32);
    }

    void fileNames()
    {
        int group = -1;
        int index = -1;
        QVERIFY(identifiersFromFileName("1000_2.png", &group, &index));
        QCOMPARE(group, 1000);
        QCOMPARE(index, 2);
        QVERIFY(identifiersFromFileName("C:/x/kfm 200-15.pcx", &group, &index));
        QCOMPARE(group, 200);
        QCOMPARE(index, 15);
        QVERIFY(identifiersFromFileName("5,0.bmp", &group, &index));
        QCOMPARE(group, 5);
        QVERIFY(!identifiersFromFileName("stand.png", &group, &index));
        QVERIFY(!identifiersFromFileName("1_2_x.png", &group, &index));
        QVERIFY(!identifiersFromFileName("70000_1.png", &group, &index));
    }

    void imageFiles()
    {
        QTemporaryDir dir;
        Palette colors = grayscalePalette(256);
        colors[3] = {10, 20, 30, 255};
        const Image indexed = Image::indexed(3, 2, {0, 3, 3, 1, 2, 3}, colors);
        const auto png = encodePng(indexed);
        QVERIFY(png.ok());
        writeFile(dir.filePath("a.png"), toBytes(png.value()));
        QString error;
        const Image loaded = loadImageFile(dir.filePath("a.png"), &error);
        QVERIFY2(loaded.isIndexed(), qPrintable(error));
        QVERIFY(loaded.pixelBuffer() == indexed.pixelBuffer());
        QCOMPARE(loaded.palette()[3].r, std::uint8_t(10));

        QImage qt(4, 4, QImage::Format_RGB32);
        qt.fill(Qt::blue);
        QVERIFY(qt.save(dir.filePath("b.jpg"), "JPG"));
        const Image jpg = loadImageFile(dir.filePath("b.jpg"), &error);
        QVERIFY2(jpg.isRgba(), qPrintable(error));
        QCOMPARE(jpg.width(), 4);

        writeFile(dir.filePath("c.png"), "nope");
        QVERIFY(loadImageFile(dir.filePath("c.png"), &error).pixelBuffer().empty());
        QVERIFY(!error.isEmpty());
    }

    void addingSprites()
    {
        SpriteDocument doc = sample();
        Palette colors = grayscalePalette(256);
        colors[1] = {1, 2, 3, 255};
        const Image image = Image::indexed(4, 4, {0, 0, 0, 0, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1}, colors);

        AddSpritesOptions options;
        options.group = 50;
        options.index = 0;
        options.indexStep = 10;
        options.axisAnchor = 7;
        QList<int> added = addSprites(doc, 0, {{"a.png", image}, {"b.png", image}}, options);
        QCOMPARE(added, (QList<int>{1, 2}));
        QCOMPARE(doc.sprites()[1].number(), 0);
        QCOMPARE(doc.sprites()[2].number(), 10);
        QCOMPARE(int(doc.sprites()[1].header.axisX), 2);
        QCOMPARE(int(doc.sprites()[1].header.axisY), 4);
        QCOMPARE(doc.palettes().size(), 2);
        QCOMPARE(doc.sprites()[1].palette, doc.sprites()[2].palette);

        options.detectFromFileName = true;
        options.index = 100;
        added = addSprites(doc, doc.spriteCount() - 1, {{"9_3.png", image}, {"x.png", image}}, options);
        QCOMPARE(doc.sprites()[added[0]].group(), 9);
        QCOMPARE(doc.sprites()[added[0]].number(), 3);
        QCOMPARE(doc.sprites()[added[1]].number(), 100);

        options = {};
        options.axisAnchor = 0;
        options.crop = AddSpritesOptions::Crop::BeforeAxis;
        added = addSprites(doc, -1, {{"c.png", image}}, options);
        QCOMPARE(added, QList<int>{0});
        QCOMPARE(doc.sprites()[0].width(), 3);
        QCOMPARE(int(doc.sprites()[0].header.axisX), 0);
        options.crop = AddSpritesOptions::Crop::AfterAxis;
        added = addSprites(doc, -1, {{"d.png", image}}, options);
        QCOMPARE(doc.sprites()[0].width(), 3);
        QCOMPARE(int(doc.sprites()[0].header.axisX), -1);
        QCOMPARE(int(doc.sprites()[0].header.axisY), -1);

        options = {};
        options.palette = AddSpritesOptions::Palette::Exchange;
        options.paletteGroup = 1;
        options.paletteIndex = 1;
        added = addSprites(doc, -1, {{"e.png", image}}, options);
        QCOMPARE(doc.sprites()[0].palette, doc.palettes()[0]);
        QCOMPARE(doc.image(0).indexAt(1, 1), std::uint8_t(1));
        options.palette = AddSpritesOptions::Palette::Force;
        added = addSprites(doc, -1, {{"f.png", image}}, options);
        QCOMPARE(doc.sprites()[0].palette, doc.palettes()[0]);
        QCOMPARE(doc.image(0).indexAt(1, 1), std::uint8_t(4));
        QCOMPARE(doc.image(0).indexAt(0, 0), std::uint8_t(0));
    }

    void spriteFile()
    {
        QTemporaryDir dir;
        Test::FixtureFactory factory(3);
        const auto written = factory.sff(SffVersion::V2, 12).write();
        QVERIFY(written.ok());
        const QByteArray original = toBytes(written.value());
        const QString path = dir.filePath("chars.sff");
        writeFile(path, original);

        SpriteFile file(path, "sprites");
        QString error;
        QVERIFY2(file.load(&error), qPrintable(error));
        QCOMPARE(file.document().spriteCount(), 12);
        QVERIFY(!file.isModified());
        QSignalSpy modified(&file, &ProjectFile::modifiedChanged);
        QSignalSpy changed(&file, &SpriteFile::documentChanged);

        SpriteDocument edited = file.document();
        SpriteEdits::setAxis(edited, 0, 99, 98);
        file.apply("Axis", edited);
        QVERIFY(file.isModified());
        QCOMPARE(int(file.document().sprites()[0].header.axisX), 99);
        QCOMPARE(file.undoStack()->undoText(), QStringLiteral("Axis"));
        file.undoStack()->undo();
        QVERIFY(!file.isModified());
        QCOMPARE(modified.count(), 2);
        QCOMPARE(changed.count(), 2);

        const QString copy = dir.filePath("copy.sff");
        QVERIFY(file.writeCopy(copy, &error));
        QFile copied(copy);
        QVERIFY(copied.open(QIODevice::ReadOnly));
        QCOMPARE(copied.readAll(), original);
        copied.close();
        file.undoStack()->redo();
        QVERIFY(file.save(&error));
        QVERIFY(!file.isModified());
        SpriteFile reread(path, "sprites");
        QVERIFY(reread.load(&error));
        QCOMPARE(int(reread.document().sprites()[0].header.axisX), 99);

        const QString v1 = dir.filePath("old.sff");
        QVERIFY2(file.saveAs(v1, SffVersion::V1, &error), qPrintable(error));
        QCOMPARE(file.path(), QDir::fromNativeSeparators(v1));
        QCOMPARE(file.document().version(), SffVersion::V1);
        QCOMPARE(file.document().spriteCount(), 12);
        SpriteFile converted(v1, "sprites");
        QVERIFY(converted.load(&error));
        QCOMPARE(converted.document().version(), SffVersion::V1);

        file.undoStack()->clear();
        const int steps = file.undoStack()->count();
        for (int dx : {1, 1, -1}) {
            SpriteDocument moved = file.document();
            SpriteEdits::moveAxes(moved, {2}, dx, 0);
            file.applyAxisChange(2, moved);
        }
        QCOMPARE(file.undoStack()->count(), steps + 1);
        SpriteDocument other = file.document();
        SpriteEdits::moveAxes(other, {3}, 0, 1);
        file.applyAxisChange(3, other);
        QCOMPARE(file.undoStack()->count(), steps + 2);
        const int x = file.document().sprites()[2].header.axisX;
        file.undoStack()->undo();
        file.undoStack()->undo();
        QCOMPARE(int(file.document().sprites()[2].header.axisX), x - 1);

        writeFile(dir.filePath("bad.sff"), "not a sprite file");
        SpriteFile bad(dir.filePath("bad.sff"), "sprites");
        QVERIFY(!bad.load(&error));
        QVERIFY(error.contains("bad.sff"));
    }
};

QTEST_GUILESS_MAIN(SpriteEditsTest)
#include "SpriteEditsTest.moc"
