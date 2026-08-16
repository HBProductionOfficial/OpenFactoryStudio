#include "core/sprites/SpriteEdits.h"
#include "core/sprites/SpriteDocument.h"
#include "core/sprites/SpriteFile.h"
#include "fixtures/FixtureFactory.h"

#include <QFile>
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

    void duplicates()
    {
        SpriteDocument doc = sample();
        QCOMPARE(SpriteEdits::freeNumber(doc, 0), 2);
        QCOMPARE(SpriteEdits::freeNumber(doc, 1, 0), 1);
        const int copy = SpriteEdits::duplicate(doc, 0);
        QCOMPARE(copy, 1);
        QCOMPARE(doc.sprites()[1].number(), 2);
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
        QCOMPARE(doc.sprites()[3].palette, doc.sprites()[4].palette);
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

        writeFile(dir.filePath("bad.sff"), "not a sprite file");
        SpriteFile bad(dir.filePath("bad.sff"), "sprites");
        QVERIFY(!bad.load(&error));
        QVERIFY(error.contains("bad.sff"));
    }
};

QTEST_GUILESS_MAIN(SpriteEditsTest)
#include "SpriteEditsTest.moc"
