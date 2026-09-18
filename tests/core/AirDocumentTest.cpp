#include "core/anims/AirDocument.h"
#include "core/anims/AirEdits.h"
#include "core/anims/AirGif.h"
#include "core/sprites/SpriteDocument.h"
#include "formats/codecs/Gif.h"

#include <QTest>
#include <QTextDocument>

using namespace OFS;

namespace {

// Two animations, 0 "Stand" and 20 "Walk". Line numbers on the right.
QStringList sample()
{
    return {
        "; Stand",                       // 0
        "[Begin Action 0]",              // 1
        "Clsn2Default: 2",               // 2
        " Clsn2[0] = -10,  0, 19,-80",   // 3
        " Clsn2[1] = -5, -80, 5, -90",   // 4
        "0,0, 0,0, 10",                  // 5
        "0,1, 0,0, 7 ; trailing",        // 6
        "Clsn1: 1",                      // 7
        "  Clsn1[0] = 10, -60, 40, -50", // 8
        "Loopstart",                     // 9
        "0,2, 0,0, 7, H",                // 10
        "",                              // 11
        "; Walk",                        // 12
        "[Begin Action 20]",             // 13
        "20,0, 0,0, 5",                  // 14
        "20,1, 0,0, -1",                 // 15
        "",                              // 16
    };
}

} // namespace

class AirDocumentTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void parsing()
    {
        const AirDocument doc = AirDocument::parse(sample());
        QCOMPARE(doc.count(), 2);
        QVERIFY(doc.problems().isEmpty());
        const AirAnimation& stand = doc.animations().at(0);
        QCOMPARE(stand.number, 0);
        QCOMPARE(stand.name, QStringLiteral("Stand"));
        QCOMPARE(stand.caption(), QStringLiteral("0 - Stand"));
        QCOMPARE(stand.nameLine, 0);
        QCOMPARE(stand.headerLine, 1);
        QCOMPARE(stand.lastLine, 10);
        QCOMPARE(stand.endLine, 12);
        QCOMPARE(stand.frames.size(), 3);
        QCOMPARE(stand.frames.at(0).blocks.size(), 1);
        QVERIFY(stand.frames.at(0).blocks.at(0).isDefault);
        QCOMPARE(stand.frames.at(0).blocks.at(0).boxes.size(), 2);
        QCOMPARE(stand.frames.at(0).blocks.at(0).boxes.at(0).bottom, -80);
        QCOMPARE(stand.frames.at(1).comment, QStringLiteral("; trailing"));
        QCOMPARE(stand.frames.at(1).sectionStart, 6);
        QVERIFY(stand.frames.at(2).loopStart);
        QVERIFY(stand.frames.at(2).flipH);
        QCOMPARE(stand.loopStartFrame(), 2);
        bool isDefault = false;
        bool inherited = false;
        QCOMPARE(stand.boxes(0, 2, &isDefault, &inherited).size(), 2);
        QVERIFY(isDefault && !inherited);
        QCOMPARE(stand.boxes(1, 2, &isDefault, &inherited).size(), 2);
        QVERIFY(isDefault && inherited);
        QCOMPARE(stand.boxes(2, 1, &isDefault).size(), 1);
        QVERIFY(!isDefault);
        QVERIFY(stand.boxes(1, 1).isEmpty());
        QCOMPARE(stand.endTick(1), 17);
        QCOMPARE(doc.animations().at(1).endTick(1), -1);
        int a = -1;
        int f = -1;
        QVERIFY(doc.locate(15, &a, &f));
        QCOMPARE(a, 1);
        QCOMPARE(f, 1);
        QVERIFY(doc.locate(12, &a, &f));
        QCOMPARE(f, -1);

        const AirDocument bad = AirDocument::parse(QStringList{"[Begin Action 1]", "x0,0, 1,2, 4", "0,0, 0,0, 3"});
        QCOMPARE(bad.problems().size(), 1);
        QCOMPARE(bad.problems().first().message, QStringLiteral("Unknown identifier 'x0' for animation"));
        QCOMPARE(bad.animations().first().frames.size(), 1);
    }

    void frameText()
    {
        AirFrame f;
        f.group = 0;
        f.number = 1;
        f.time = 7;
        f.flipH = true;
        QCOMPARE(f.text(), QStringLiteral("0,1, 0,0, 7, H"));
        f.flipH = false;
        f.transparency.mode = AirTransparency::Mode::Addition;
        f.number = 4;
        QCOMPARE(f.text(), QStringLiteral("0,4, 0,0, 7, , A"));
        f.transparency = {};
        f.number = 2;
        f.xScale = 2;
        f.yScale = 0.5;
        f.angle = 45;
        QCOMPARE(f.text(), QStringLiteral("0,2, 0,0, 7, , , 2,0.5, 45"));
        f.xScale = f.yScale = 1;
        f.angle = 12.3456;
        f.number = 1;
        QCOMPARE(f.text(), QStringLiteral("0,1, 0,0, 7, , , 1,1, 12.3456"));
        AirFrame g;
        g.group = -1;
        g.x = 3;
        g.y = 4;
        g.time = 7;
        g.flipV = true;
        g.transparency = AirTransparency{AirTransparency::Mode::Addition, 100, 200};
        g.xScale = 1.5;
        g.yScale = 2;
        g.angle = 10;
        QCOMPARE(g.text(), QStringLiteral("-1,0, 3,4, 7, V, AS100D200, 1.5,2, 10"));
        g = AirFrame{};
        g.flipH = g.flipV = true;
        g.group = 1;
        QCOMPARE(g.text(), QStringLiteral("1,0, 0,0, 0, HV"));

        QCOMPARE((AirTransparency{AirTransparency::Mode::Addition50, 256, 128}).text(), QStringLiteral("A1"));
        QCOMPARE((AirTransparency{AirTransparency::Mode::Addition50, 50, 128}).text(), QStringLiteral("AS50D128"));
        QCOMPARE((AirTransparency{AirTransparency::Mode::Subtraction, 10, 20}).text(), QStringLiteral("S"));
        QCOMPARE(AirTransparency::parse("as128d64").source, 128);
        QCOMPARE(AirTransparency::parse("A1").mode, AirTransparency::Mode::Addition50);
        QCOMPARE(AirTransparency::parse("AS128D64").mode, AirTransparency::Mode::Addition);
    }

    void headerAndFrameEdits()
    {
        QStringList lines = sample();
        AirEdits::setName(lines, 0, "Stand2");
        QCOMPARE(lines.at(0), QStringLiteral("; Stand2"));
        AirEdits::setName(lines, 0, QString());
        QCOMPARE(lines.at(0), QStringLiteral("[Begin Action 0]"));
        AirEdits::setName(lines, 0, "Back");
        QCOMPARE(lines.at(0), QStringLiteral("; Back"));
        AirEdits::setNumber(lines, 1, 21);
        QCOMPARE(lines.at(13), QStringLiteral("[Begin Action 21]"));

        lines = sample();
        AirFrame f = AirDocument::parse(lines).animations().at(0).frames.at(1);
        f.time = 8;
        AirEdits::setFrame(lines, 0, 1, f);
        QCOMPARE(lines.at(6), QStringLiteral("0,1, 0,0, 8 ; trailing"));

        lines = sample();
        AirEdits::setLoopStart(lines, 0, 1, true);
        QCOMPARE(lines.at(6), QStringLiteral("LoopStart"));
        QCOMPARE(lines.at(7), QStringLiteral("0,1, 0,0, 7 ; trailing"));
        QVERIFY(!lines.contains(QStringLiteral("Loopstart")));
        AirEdits::setLoopStart(lines, 0, 1, false);
        QCOMPARE(lines, [] {
            QStringList l = sample();
            l.removeAt(9);
            return l;
        }());

        lines = sample();
        AirEdits::setInterpolation(lines, 0, 2, AirFrame::InterpolateOffset, true);
        QCOMPARE(lines.mid(9, 3), (QStringList{"Interpolate Offset", "Loopstart", "0,2, 0,0, 7, H"}));
        AirEdits::setInterpolation(lines, 0, 2, AirFrame::InterpolateBlend, true);
        QCOMPARE(lines.at(10), QStringLiteral("Interpolate Blend"));
        QCOMPARE(AirDocument::parse(lines).animations().at(0).frames.at(2).interpolate,
                 AirFrame::InterpolateOffset | AirFrame::InterpolateBlend);
        AirEdits::setInterpolation(lines, 0, 2, AirFrame::InterpolateOffset, false);
        QVERIFY(!lines.contains(QStringLiteral("Interpolate Offset")));

        lines = sample();
        AirEdits::applyToNext(lines, 0, 0, AirEdits::Field::Time);
        QCOMPARE(lines.at(6), QStringLiteral("0,1, 0,0, 10 ; trailing"));
        QCOMPARE(lines.at(10), QStringLiteral("0,2, 0,0, 10, H"));
        lines = sample();
        AirEdits::applyToNext(lines, 0, 1, AirEdits::Field::Flip);
        QCOMPARE(lines.at(10), QStringLiteral("0,2, 0,0, 7"));
    }

    void importAnimations()
    {
        const QStringList source = {"; Standing", "[Begin Action 0]",  "0,0, 0,0, 10", "",
                                    "; Walking",  "[Begin Action 20]", "20,0, 0,0, 5"};
        QStringList lines = {"; Old", "[Begin Action 0]", "9,9, 0,0, 1", "", "[Begin Action 6]", "6,0, 0,0, 4"};
        using AirEdits::ImportConflict;

        QStringList added = lines;
        AirEdits::AnimationImportResult result =
            AirEdits::importAnimations(added, 0, source, {{1, 7777}, {0, 5}}, ImportConflict::Ask);
        QCOMPARE(result.added, (QList<int>{1, 2}));
        QCOMPARE(result.replaced, 0);
        QCOMPARE(added, (QStringList{"; Old", "[Begin Action 0]", "9,9, 0,0, 1", "", "; Walking", "[Begin Action 7777]",
                                     "20,0, 0,0, 5", "", "; Standing", "[Begin Action 5]", "0,0, 0,0, 10", "",
                                     "[Begin Action 6]", "6,0, 0,0, 4"}));

        QStringList replaced = lines;
        result = AirEdits::importAnimations(replaced, 1, source, {{0, 0}}, ImportConflict::Replace);
        QCOMPARE(result.replaced, 1);
        QCOMPARE(replaced.mid(0, 3), (QStringList{"; Standing", "[Begin Action 0]", "0,0, 0,0, 10"}));
        QStringList skipped = lines;
        result = AirEdits::importAnimations(skipped, 1, source, {{0, 6}}, ImportConflict::Skip);
        QVERIFY(result.added.isEmpty());
        QCOMPARE(skipped, lines);
        int asked = 0;
        result = AirEdits::importAnimations(skipped, 1, source, {{0, 6}}, ImportConflict::Ask, [&asked](int number) {
            asked = number;
            return false;
        });
        QCOMPARE(asked, 6);
        QCOMPARE(skipped, lines);
        QStringList twice = lines;
        result = AirEdits::importAnimations(twice, -1, source, {{0, 6}}, ImportConflict::AsIs);
        QCOMPARE(result.added, QList<int>{2});
        QCOMPARE(twice.last(), QStringLiteral(""));
        QCOMPARE(AirDocument::parse(twice).count(), 3);
    }

    void animationEdits()
    {
        QStringList lines = sample();
        AirFrame frame = AirEdits::blankFrame(0);
        QCOMPARE(AirEdits::addAnimation(lines, 0, 5, "New", {frame}), 1);
        QCOMPARE(lines.mid(10, 6),
                 (QStringList{"0,2, 0,0, 7, H", "", "; New", "[Begin Action 5]", "-1,0, 0,0, 0", ""}));
        QCOMPARE(AirDocument::parse(lines).animations().at(1).number, 5);
        lines = sample();
        QCOMPARE(AirEdits::addAnimation(lines, -1, 6, QString(), {AirEdits::blankFrame(4)}), 2);
        QCOMPARE(lines.mid(15), (QStringList{"20,1, 0,0, -1", "", "[Begin Action 6]", "-1,0, 0,0, 4", ""}));

        lines = sample();
        AirEdits::deleteAnimation(lines, 0);
        QCOMPARE(lines.first(), QStringLiteral("; Walk"));
        QCOMPARE(lines.size(), 5);

        lines = sample();
        QCOMPARE(AirEdits::duplicateAnimation(lines, 0, 7), 1);
        QCOMPARE(lines.at(12), QStringLiteral("; Stand"));
        QCOMPARE(lines.at(13), QStringLiteral("[Begin Action 7]"));
        QCOMPARE(lines.mid(14, 9), sample().mid(2, 9));
        QCOMPARE(lines.at(23), QString());
        QCOMPARE(lines.at(24), QStringLiteral("; Walk"));
        const AirDocument doc = AirDocument::parse(lines);
        QCOMPARE(doc.count(), 3);
        QCOMPARE(doc.animations().at(1).frames.size(), 3);

        lines = sample();
        QCOMPARE(AirEdits::animationText(lines, 1),
                 (QStringList{"; Walk", "[Begin Action 20]", "20,0, 0,0, 5", "20,1, 0,0, -1"}));
        AirEdits::replaceAnimation(lines, 1, {"; Run", "[Begin Action 20]", "21,0, 0,0, 4 ; fast"});
        QCOMPARE(lines.mid(12), (QStringList{"; Run", "[Begin Action 20]", "21,0, 0,0, 4 ; fast", ""}));

        lines = sample();
        QCOMPARE(AirEdits::insertFrames(lines, 0, 0, {AirEdits::blankFrame()}), 1);
        QCOMPARE(lines.at(6), QStringLiteral("-1,0, 0,0, 10"));
        QCOMPARE(lines.at(7), QStringLiteral("0,1, 0,0, 7 ; trailing"));
        lines = sample();
        AirEdits::deleteFrame(lines, 0, 2);
        QCOMPARE(lines.mid(6, 3), (QStringList{"0,1, 0,0, 7 ; trailing", "", "; Walk"}));
    }

    void boxEdits()
    {
        QStringList lines = sample();
        AirEdits::BoxRef ref = AirEdits::addBox(lines, 0, 1, 1, AirBox{29, 5, 0, -75});
        QCOMPARE(ref, (AirEdits::BoxRef{0, 0}));
        QCOMPARE(lines.mid(6, 3), (QStringList{"Clsn1: 1", "  Clsn1[0] = 0, -75, 29, 5", "0,1, 0,0, 7 ; trailing"}));
        lines = sample();
        ref = AirEdits::addBox(lines, 0, 0, 2, AirBox{1, 2, 3, 4});
        QCOMPARE(ref, (AirEdits::BoxRef{0, 2}));
        QCOMPARE(lines.at(2), QStringLiteral("Clsn2Default: 3"));
        QCOMPARE(lines.at(5), QStringLiteral("  Clsn2[2] = 1, 2, 3, 4"));

        lines = sample();
        AirEdits::setBox(lines, 0, 0, {0, 0}, AirBox{0, 5, 29, -75});
        QCOMPARE(lines.at(3), QStringLiteral("  Clsn2[0] = 0, -75, 29, 5"));

        lines = sample();
        AirEdits::deleteBoxes(lines, 0, 0, {{0, 0}});
        QCOMPARE(lines.mid(2, 2), (QStringList{"Clsn2Default: 1", " Clsn2[0] = -5, -80, 5, -90"}));
        lines = sample();
        AirEdits::deleteBoxes(lines, 0, 2, {{0, 0}});
        QCOMPARE(lines.mid(6, 3), (QStringList{"0,1, 0,0, 7 ; trailing", "Loopstart", "0,2, 0,0, 7, H"}));
        lines = sample();
        AirEdits::deleteFrameBoxes(lines, 0, 0);
        QCOMPARE(lines.at(2), QStringLiteral("0,0, 0,0, 10"));
        lines = sample();
        AirEdits::deleteAnimationBoxes(lines, 0);
        QCOMPARE(lines.size(), sample().size() - 5);

        lines = sample();
        ref = AirEdits::switchBoxType(lines, 0, 2, {0, 0});
        QCOMPARE(ref, (AirEdits::BoxRef{0, 0}));
        QCOMPARE(lines.mid(7, 2), (QStringList{"Clsn2: 1", "  Clsn2[0] = 10, -60, 40, -50"}));
        lines = sample();
        AirEdits::switchBoxDefault(lines, 0, 2, {0, 0});
        QCOMPARE(lines.at(7), QStringLiteral("Clsn1Default: 1"));

        lines = sample();
        AirEdits::copyPreviousBoxes(lines, 0, 1);
        QCOMPARE(lines.mid(6, 4), sample().mid(2, 3) << QStringLiteral("0,1, 0,0, 7 ; trailing"));
        lines = sample();
        AirEdits::copyPreviousBoxes(lines, 0, 2);
        QCOMPARE(lines.mid(7, 3),
                 (QStringList{"Clsn2: 2", "  Clsn2[0] = -10, -80, 19, 0", "  Clsn2[1] = -5, -90, 5, -80"}));

        lines = sample();
        lines.insert(16, QStringLiteral("0,2, -5,0, 3"));
        QCOMPARE(AirEdits::replicateBoxes(lines, 0, 2), 1);
        QCOMPARE(lines.mid(16, 3), (QStringList{"Clsn1: 1", "  Clsn1[0] = 10, -60, 40, -50", "0,2, -5,0, 3"}));

        lines = sample();
        AirEdits::pasteBoxes(lines, 0, 2, 1, {AirBox{1, 1, 2, 2}});
        QCOMPARE(lines.at(7), QStringLiteral("Clsn1: 2"));
        QCOMPARE(lines.at(9), QStringLiteral("  Clsn1[1] = 1, 1, 2, 2"));
    }

    void alignment()
    {
        QStringList lines = {"[Begin Action 1]", "Clsn1: 1", "  Clsn1[0] = -12, -10, 19, 10", "0,0, 0,0, 5"};
        QStringList scaled = lines;
        AirEdits::align(scaled, {0}, AirEdits::AlignOperation::ScaleBoxes, 200, 100);
        QCOMPARE(scaled.at(2), QStringLiteral("  Clsn1[0] = -24, -10, 39, 10"));
        scaled = lines;
        AirEdits::align(scaled, {0}, AirEdits::AlignOperation::ScaleBoxes, 2, 3);
        const AirBox tiny = AirDocument::parse(scaled).animations().first().frames.first().blocks.first().boxes.first();
        QVERIFY(tiny.right >= tiny.left && tiny.bottom >= tiny.top);
        QStringList moved = lines;
        AirEdits::align(moved, {0}, AirEdits::AlignOperation::OffsetAxis, 3, -2);
        QCOMPARE(moved.at(3), QStringLiteral("0,0, 3,-2, 5"));
        AirEdits::align(moved, {0}, AirEdits::AlignOperation::ResizeBottomRight, 1, 1);
        QCOMPARE(moved.at(2), QStringLiteral("  Clsn1[0] = -12, -10, 20, 11"));
    }

    void optimizing()
    {
        QStringList lines = {"[Begin Action 1]", "Clsn1: 1", "  Clsn1[0] = 0, 0, 5, 5",
                             "0,0, 0,0, 5",      "Clsn1: 1", "  Clsn1[0] = 0, 0, 5, 5",
                             "0,1, 0,0, 5",      "Clsn1: 1", "  Clsn1[0] = 1, 1, 2, 2",
                             "0,2, 0,0, 5"};
        QCOMPARE(AirEdits::optimize(lines), 2);
        QCOMPARE(lines.at(1), QStringLiteral("Clsn1Default: 1"));
        QCOMPARE(lines.at(3), QStringLiteral("0,0, 0,0, 5"));
        QCOMPARE(lines.at(4), QStringLiteral("0,1, 0,0, 5"));
        lines = {"[Begin Action 1]", "Clsn1: 1",   "  Clsn1[0] = 0, 0, 5, 5",
                 "0,0, 0,0, 5",      "Clsn1: 1",   "  Clsn1[0] = 0, 0, 5, 5",
                 "0,1, 0,0, 5",      "0,2, 0,0, 5"};
        const QStringList unchanged = lines;
        QCOMPARE(AirEdits::optimize(lines), 0);
        QCOMPARE(lines, unchanged);
        lines = {"[Begin Action 1]", "0,0, 0,0, 5", "", "; Old", ";[Begin Action 44]", ";0,0, 0,0, 3", ";Clsn2: 1", "",
                 "[Begin Action 2]", "0,0, 0,0, 5"};
        QCOMPARE(AirEdits::optimize(lines), 5);
        QCOMPARE(lines, (QStringList{"[Begin Action 1]", "0,0, 0,0, 5", "", "[Begin Action 2]", "0,0, 0,0, 5"}));
    }

    void gif()
    {
        // Running total at 60 ticks per second, rounded down. An endless frame is 65535 and ends the list.
        QCOMPARE(gifDelays({12, 7, 7, 7, 7, 45, 7, 7, 7, 7, 40}, 1.0),
                 (QList<int>{20, 11, 12, 12, 11, 75, 12, 12, 11, 12, 67}));
        QCOMPARE(gifDelays({3, 3, 3, 3}, 2.0), (QList<int>{2, 3, 2, 3}));
        QCOMPARE(gifDelays({3}, 0.5), QList<int>{10});
        QCOMPARE(gifDelays({2, 1, 4, 3, 2}, 1.0), (QList<int>{3, 2, 6, 5, 4}));
        QCOMPARE(gifDelays({5, -1, 5}, 1.0), (QList<int>{8, 65535}));

        SpriteDocument sprites;
        sprites.palettes() << SpriteDocument::makePalette(1, 1, Formats::grayscalePalette(256));
        SpriteEntry entry;
        entry.header.axisX = 2;
        entry.header.axisY = 4;
        entry.pixels = SpriteDocument::makePixels(Formats::Image::indexed(4, 4, std::vector<std::uint8_t>(16, 7)));
        entry.palette = sprites.palettes()[0];
        sprites.sprites() << entry;
        const AirDocument doc = AirDocument::parse(QStringList{"[Begin Action 0]", "0,0, 0,0, 3", "0,0, 10,0, 3, H"});
        QString error;
        const QByteArray bytes = animationGif(doc.animations().first(), sprites, 2.0, 1.0, &error);
        QVERIFY2(!bytes.isEmpty(), qPrintable(error));
        const auto gif = Formats::readGif(
            Formats::ByteSpan(reinterpret_cast<const std::byte*>(bytes.constData()), std::size_t(bytes.size())));
        QVERIFY(gif.ok());
        QCOMPARE(gif.value().width, 28); // units -2..12 (14) at scale 2
        QCOMPARE(gif.value().height, 8);
        QCOMPARE(gif.value().loopCount, 0);
        QCOMPARE(gif.value().frames.size(), std::size_t(2));
        QCOMPARE(gif.value().frames.at(0).delay, 5);
        QCOMPARE(gif.value().frames.at(1).left, 20); // the flipped frame: units 8..12, from -2 at scale 2
        QVERIFY(!gif.value().globalPalette.empty());

        const AirDocument endless =
            AirDocument::parse(QStringList{"[Begin Action 0]", "0,0, 0,0, 3", "0,0, 0,0, -1", "0,0, 10,0, 3"});
        const QByteArray once = animationGif(endless.animations().first(), sprites, 1.0, 1.0, &error);
        const auto held = Formats::readGif(
            Formats::ByteSpan(reinterpret_cast<const std::byte*>(once.constData()), std::size_t(once.size())));
        QVERIFY(held.ok());
        QCOMPARE(held.value().loopCount, -1);
        QVERIFY(!once.contains("NETSCAPE2.0"));
        QCOMPARE(held.value().frames.size(), std::size_t(2));
        QCOMPARE(held.value().frames.at(1).delay, 65535);
    }

    void documentChanges()
    {
        QTextDocument document;
        document.setPlainText(sample().join('\n'));
        QStringList lines = AirEdits::lines(&document);
        QCOMPARE(lines, sample());
        AirEdits::setBox(lines, 0, 0, {0, 0}, AirBox{0, 5, 29, -75});
        QVERIFY(AirEdits::apply(&document, lines));
        QCOMPARE(AirEdits::lines(&document), lines);
        QVERIFY(!AirEdits::apply(&document, lines));
        AirEdits::deleteAnimation(lines, 1);
        QVERIFY(AirEdits::apply(&document, lines));
        QCOMPARE(AirEdits::lines(&document), lines);
        AirEdits::insertFrames(lines, 0, -1, {AirEdits::blankFrame()});
        QVERIFY(AirEdits::apply(&document, lines));
        QCOMPARE(AirEdits::lines(&document), lines);
        AirEdits::addAnimation(lines, -1, 3, "End", {AirEdits::blankFrame()});
        QVERIFY(AirEdits::apply(&document, lines));
        QCOMPARE(AirEdits::lines(&document), lines);
        AirEdits::deleteFrame(lines, 0, 0);
        QVERIFY(AirEdits::apply(&document, lines));
        QCOMPARE(AirEdits::lines(&document), lines);
        for (int i = 0; i < 4; ++i)
            document.undo();
        QVERIFY(AirEdits::lines(&document) != sample());
        document.undo();
        QCOMPARE(AirEdits::lines(&document), sample());
        QVERIFY(!document.isUndoAvailable());
    }
};

QTEST_MAIN(AirDocumentTest)
#include "AirDocumentTest.moc"
