#include "core/sprites/SpriteDocument.h"
#include "fixtures/FixtureFactory.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QTest>

#include <algorithm>

using namespace OFS;
using namespace OFS::Formats;

namespace {

SffDocument reread(const SffDocument& sff)
{
    auto bytes = sff.write();
    if (!bytes)
        return {};
    auto doc = SffDocument::read(ByteSpan(bytes.value()));
    return doc ? std::move(doc.value()) : SffDocument();
}

struct Look
{
    int group, number, x, y;
    Image pixels;
    friend bool operator==(const Look&, const Look&) = default;
};

std::vector<Look> looks(const SffDocument& sff)
{
    std::vector<Look> result;
    for (std::size_t i = 0; i < sff.sprites.size(); ++i) {
        const SffSprite& s = sff.sprites[i];
        auto rgba = sff.decodeSpriteRgba(i);
        result.push_back({s.group, s.number, s.axisX, s.axisY, rgba ? rgba.value() : Image()});
    }
    return result;
}

} // namespace

class SpriteDocumentTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void unchangedRoundTrip_data()
    {
        QTest::addColumn<int>("version");
        QTest::addColumn<quint64>("seed");
        for (quint64 seed : {1ull, 2ull, 3ull}) {
            QTest::addRow("v1 seed %llu", seed) << int(SffVersion::V1) << seed;
            QTest::addRow("v2 seed %llu", seed) << int(SffVersion::V2) << seed;
            QTest::addRow("v2.01 seed %llu", seed) << int(SffVersion::V201) << seed;
        }
    }

    void unchangedRoundTrip()
    {
        QFETCH(int, version);
        QFETCH(quint64, seed);
        Test::FixtureFactory factory(seed);
        const SffDocument original = reread(factory.sff(SffVersion(version), 40));
        QVERIFY(!original.sprites.empty());
        const auto bytes = original.write();
        QVERIFY(bytes.ok());

        const SpriteDocument model = SpriteDocument::fromSff(original);
        QCOMPARE(model.spriteCount(), int(original.sprites.size()));
        QCOMPARE(model.palettes().size(), qsizetype(original.palettes.size()));
        const auto again = model.toSff().write();
        QVERIFY(again.ok());
        QVERIFY(again.value() == bytes.value());

        for (int i = 0; i < model.spriteCount(); ++i) {
            auto expected = original.decodeSpriteRgba(std::size_t(i));
            QVERIFY(expected.ok());
            QVERIFY2(model.imageRgba(i) == expected.value(), qPrintable(QString::number(i)));
        }
    }

    void linksSurviveEdits()
    {
        Test::FixtureFactory factory(7);
        const SffDocument original = reread(factory.sff(SffVersion::V2, 40));
        SpriteDocument model = SpriteDocument::fromSff(original);

        int owner = -1;
        for (int i = 0; i < model.spriteCount() && owner < 0; ++i) {
            if (model.linkedSprites(i).size() > 1)
                owner = i;
        }
        QVERIFY2(owner >= 0, "the fixture has linked sprites");
        const QList<int> linked = model.linkedSprites(owner);
        QCOMPARE(linked.first(), owner);

        std::vector<Look> expected = looks(original);
        expected.erase(expected.begin() + owner);
        model.sprites().removeAt(owner);
        SffDocument written = reread(model.toSff());
        QCOMPARE(written.sprites.size(), expected.size());
        QVERIFY(looks(written) == expected);
        QVERIFY(!written.sprites[std::size_t(linked[1] - 1)].isLinked());

        const int link = linked.size() > 2 ? linked[2] - 1 : linked[1] - 1;
        std::rotate(expected.begin(), expected.begin() + link, expected.begin() + link + 1);
        model.sprites().move(link, 0);
        written = reread(model.toSff());
        QVERIFY(looks(written) == expected);
        QVERIFY(!written.sprites[0].isLinked());
    }

    void palettesFollowSprites()
    {
        Test::FixtureFactory factory(11);
        const SffDocument original = reread(factory.sff(SffVersion::V2, 30));
        SpriteDocument model = SpriteDocument::fromSff(original);
        QVERIFY(model.palettes().size() > 1);
        const std::vector<Look> expected = looks(original);

        std::reverse(model.palettes().begin(), model.palettes().end());
        SffDocument written = reread(model.toSff());
        QVERIFY(looks(written) == expected);

        const SpritePalettePtr used = model.sprites().first().palette;
        QVERIFY(used);
        model.palettes().removeAll(used);
        written = reread(model.toSff());
        QVERIFY(looks(written) == expected);
        QCOMPARE(written.palettes.size(), std::size_t(model.palettes().size() + 1));
    }

    // SFF v2.0 keeps a fourth byte per color that is not alpha. M.U.G.E.N 1.0 leaves it unset.
    void palettesWithoutAlpha()
    {
        Test::FixtureFactory factory(5);
        SffDocument file = factory.sff(SffVersion::V2, 10);
        QVERIFY(!file.palettes.empty());
        const std::uint8_t junk[] = {124, 0, 94, 120};
        for (SffPalette& p : file.palettes) {
            for (std::size_t c = 0; c < p.colors.size(); ++c)
                p.colors[c].a = junk[c % 4];
        }
        file = reread(file);
        const auto bytes = file.write();
        QVERIFY(bytes.ok());
        const SpriteDocument model = SpriteDocument::fromSff(file);
        const Palette& colors = model.palettes().first()->palette.colors;
        QVERIFY(colors.size() > 2);
        QCOMPARE(int(colors[0].a), 0);
        for (std::size_t c = 1; c < colors.size(); ++c)
            QCOMPARE(int(colors[c].a), 255);
        const auto again = model.toSff().write();
        QVERIFY(again.ok());
        QVERIFY(again.value() == bytes.value());

        SpriteDocument edited = model;
        edited.palettes().first() = SpriteDocument::makePalette(1, 1, colors);
        const SffDocument written = reread(edited.toSff());
        QCOMPARE(int(written.palettes.front().colors[1].a), 255);

        SffDocument v201 = factory.sff(SffVersion::V201, 10);
        QVERIFY(!v201.palettes.empty() && v201.palettes.front().colors.size() > 1);
        v201.palettes.front().colors[1].a = 128;
        v201 = reread(v201);
        QCOMPARE(int(SpriteDocument::fromSff(v201).palettes().first()->palette.colors[1].a), 128);
    }

    void corpusRoundTrip()
    {
        QStringList files;
        for (const char* variable : {"OFS_CORPUS_KFM"}) {
            const QString root = qEnvironmentVariable(variable);
            if (root.isEmpty())
                continue;
            QDirIterator it(root, {"*.sff"}, QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext())
                files << it.next();
        }
        if (files.isEmpty())
            QSKIP("OFS_CORPUS_KFM is not set");
        int checked = 0;
        for (const QString& path : std::as_const(files)) {
            QFile file(path);
            QVERIFY(file.open(QIODevice::ReadOnly));
            const QByteArray data = file.readAll();
            auto sff = SffDocument::read(
                ByteSpan(reinterpret_cast<const std::byte*>(data.constData()), std::size_t(data.size())));
            if (!sff)
                continue;
            const auto original = sff.value().write();
            const auto again = SpriteDocument::fromSff(sff.value()).toSff().write();
            QVERIFY2(original.ok() && again.ok() && original.value() == again.value(), qPrintable(path));
            ++checked;
        }
        qInfo("%d sprite files", checked);
    }

    void newSprites()
    {
        Test::FixtureFactory factory(5);
        SpriteDocument model = SpriteDocument::fromSff(reread(factory.sff(SffVersion::V2, 6)));
        const Image image = factory.indexedImage(9, 7, 40, Test::PixelPattern::Noise);
        SpriteEntry entry;
        entry.header.group = 9000;
        entry.header.number = 1;
        entry.header.axisX = 4;
        entry.header.axisY = 6;
        entry.pixels = SpriteDocument::makePixels(image);
        entry.palette = SpriteDocument::makePalette(9000, 1, factory.palette(256));
        model.sprites().insert(2, entry);
        SpriteEntry copy = entry;
        copy.header.number = 2;
        model.sprites().insert(3, copy);
        QCOMPARE(model.linkedSprites(2), (QList<int>{2, 3}));
        QCOMPARE(model.findSprite(9000, 2), 3);

        const SffDocument written = reread(model.toSff());
        QCOMPARE(written.sprites.size(), std::size_t(8));
        QCOMPARE(written.sprites[2].group, std::uint16_t(9000));
        QCOMPARE(written.sprites[2].axisX, std::int16_t(4));
        QVERIFY(written.sprites[3].isLinked());
        QCOMPARE(written.sprites[3].linkedIndex, 2);
        auto decoded = written.decodeSprite(3);
        QVERIFY(decoded.ok());
        QVERIFY(decoded.value().pixelBuffer() == image.pixelBuffer());
        QVERIFY(written.findPalette(9000, 1).has_value());
    }
};

QTEST_GUILESS_MAIN(SpriteDocumentTest)
#include "SpriteDocumentTest.moc"
