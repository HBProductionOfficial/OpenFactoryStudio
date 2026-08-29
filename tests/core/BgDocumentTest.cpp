#include "core/bg/BgDocument.h"
#include "core/anims/AirDocument.h"
#include "core/bg/BgSimulator.h"

#include <QTest>

using namespace OFS;

namespace {

QStringList lines(const char* text)
{
    return QString::fromLatin1(text).split(QLatin1Char('\n'));
}

} // namespace

class BgDocumentTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void stage()
    {
        const QStringList text = lines("[Info]\nname = \"Stage\"\n\n"
                                       "[BGdef]\nspr = stage.sff\n\n"
                                       "[BG 0] ; the sky\ntype  = normal\nspriteno = 0, 1\nstart = 5, -10\n"
                                       "delta = .5,1\ntrans = AddAlpha\nalpha = 128, 64\nmask = 1\ntile = 1, 0\n"
                                       "tilespacing = 4\nwindow = 0,0, 319, 100\nsin.x = 10, 60, 15\nid = 7\n\n"
                                       "[BG Floor]\ntype = parallax\nwidth = 400, 800\nyscalestart = 90\n"
                                       "[BG Anim]\ntype = anim\nactionno = 10\nlayerno = 1\npositionlink = 1\n"
                                       "[BGCtrlDef Move]\nlooptime = 100\nctrlid = 7, 8\n"
                                       "[BGCtrl Go]\ntype = VelSet\ntime = 10, 20\nx = 2\n"
                                       "[BGCtrl Hide]\ntype = visible\ntime = 50\nvalue = 0\nctrlID = 9\n");
        const BgDocument doc = BgDocument::parse(text);
        QCOMPARE(doc.groups().size(), 1);
        const BgGroup& group = doc.groups().first();
        QCOMPARE(group.prefix, QStringLiteral("BG"));
        QCOMPARE(group.def.value(QStringLiteral("spr")).value(), QStringLiteral("stage.sff"));
        QCOMPARE(group.elements.size(), 3);

        const BgElement& sky = group.elements[0];
        QCOMPARE(sky.section.name, QStringLiteral("0"));
        QCOMPARE(sky.section.headerLine, 6);
        QCOMPARE(sky.section.lastLine, 18);
        QCOMPARE(sky.type, BgElement::Type::Normal);
        QCOMPARE(sky.spriteNumber, 1);
        QCOMPARE(sky.start, QPointF(5, -10));
        QCOMPARE(sky.delta, QPointF(0.5, 1));
        QCOMPARE(sky.trans, BgElement::Trans::AddAlpha);
        QCOMPARE(sky.alphaSource, 128);
        QCOMPARE(sky.alphaDest, 64);
        QVERIFY(sky.mask);
        QCOMPARE(sky.tileX, 1);
        QCOMPARE(sky.tileSpacingX, 4);
        QCOMPARE(sky.tileSpacingY, 0); // one value sets x only
        QVERIFY(sky.hasWindow);
        QCOMPARE(sky.window, QRect(QPoint(0, 0), QPoint(319, 100)));
        QCOMPARE(sky.sinX[1], 60.0);
        QCOMPARE(sky.id, 7);

        const BgElement& floor = group.elements[1];
        QCOMPARE(floor.type, BgElement::Type::Parallax);
        QVERIFY(floor.hasWidth && !floor.hasXScale);
        QCOMPARE(floor.widthBottom, 800.0);
        QCOMPARE(floor.yScaleStart, 90.0);
        QCOMPARE(floor.delta, QPointF(1, 1));

        const BgElement& anim = group.elements[2];
        QCOMPARE(anim.type, BgElement::Type::Animation);
        QCOMPARE(anim.action, 10);
        QCOMPARE(anim.layer, 1);
        QVERIFY(anim.mask);
        QVERIFY(anim.positionLink);

        QCOMPARE(group.controllerDefs.size(), 1);
        const BgControllerDef& def = group.controllerDefs.first();
        QCOMPARE(def.loopTime, 100);
        QCOMPARE(def.ids.value(), (QList<int>{7, 8}));
        QCOMPARE(def.controllers.size(), 2);
        QCOMPARE(def.controllers[0].type, BgController::Type::VelSet);
        QCOMPARE(def.controllers[0].startTime, 10);
        QCOMPARE(def.controllers[0].endTime, 20);
        QCOMPARE(def.controllers[0].x.value(), 2.0);
        QVERIFY(!def.controllers[0].y);
        QCOMPARE(def.controllers[1].endTime, 50);
        QCOMPARE(def.controllers[1].ids.value(), QList<int>{9});
        QVERIFY(doc.section(QStringLiteral("info")));
    }

    void motifAndStoryboard()
    {
        const BgDocument motif = BgDocument::parse(lines("[TitleBGdef]\n[TitleBG 0]\nspriteno = 5,1\n"
                                                         "[SelectBGdef]\n[SelectBG 2.A] ;title\n[TitleBG 1]\n"
                                                         "[SelectBGCtrlDef x]\n[SelectBGCtrl y]\ntype = posadd\n"));
        QCOMPARE(motif.groups().size(), 2);
        QCOMPARE(motif.indexOf(QStringLiteral("selectbg")), 1);
        QCOMPARE(motif.groups()[0].elements.size(), 2);
        QCOMPARE(motif.groups()[1].elements.first().section.name, QStringLiteral("2.A"));
        QCOMPARE(motif.groups()[1].controllerDefs.first().controllers.first().type, BgController::Type::PosAdd);

        const BgDocument story = BgDocument::parse(lines("[SceneDef]\n[Scene 0]\nbg.name = BG0\n[BG0Def]\n"
                                                         "[BG0 Mountains]\n[BG0 Wall]\n[Begin Action 10]\n"
                                                         "0,0, 0,0, 5\n"));
        QCOMPARE(story.groups().size(), 1);
        QCOMPARE(story.groups()[0].prefix, QStringLiteral("BG0"));
        QCOMPARE(story.groups()[0].elements.size(), 2);
    }

    void simulation()
    {
        const BgDocument doc = BgDocument::parse(
            lines("[BGdef]\n[BG a]\nvelocity = 1, 0.5\nid = 1\nsin.y = 4, 8, 90\nstart = 10, 20\n"
                  "[BG b]\nid = 2\nsin.x = 2, 4, 90\n[BG linked]\npositionlink = 1\nstart = 1, 1\nid = 3\n"
                  "[BGCtrlDef d]\nlooptime = 30\n"
                  "[BGCtrl v]\ntype = veladd\ntime = 5, 6\nx = 1\nctrlid = 1\n"
                  "[BGCtrl p]\ntype = posset\ntime = 10\ny = 50\nctrlid = 2\n"
                  "[BGCtrl h]\ntype = visible\ntime = 12\nvalue = 0\nctrlid = 2\n"
                  "[BGCtrl s]\ntype = enable\ntime = 20\nvalue = 0\nctrlid = 1\n"
                  "[BGCtrlDef all]\nctrlid = -1\n[BGCtrl once]\ntype = posadd\ntime = 2\nx = 100\n"));
        BgSimulator simulator(doc.groups().first());
        QCOMPARE(simulator.elements()[2].start, QPointF(1, 1));
        QCOMPARE(simulator.elements()[2].velocity, QPointF(0, 0));
        simulator.runTo(5);
        QCOMPARE(simulator.states()[0].position, QPointF(105, 2.5));
        QCOMPARE(simulator.states()[1].position, QPointF(100, 0));
        simulator.runTo(7); // VelAdd at ticks 5 and 6: the velocity becomes 3
        QCOMPARE(simulator.states()[0].velocity, QPointF(3, 0.5));
        QCOMPARE(simulator.states()[0].position, QPointF(110, 3.5));
        simulator.runTo(11);
        QCOMPARE(simulator.states()[1].position, QPointF(100, 50));
        QVERIFY(simulator.shown(1));
        simulator.runTo(13);
        QVERIFY(!simulator.shown(1));
        simulator.runTo(25); // disabled at 20: frozen and not drawn
        QVERIFY(!simulator.shown(0));
        const QPointF frozen = simulator.states()[0].position;
        simulator.runTo(28);
        QCOMPARE(simulator.states()[0].position, frozen);
        simulator.runTo(3);
        QCOMPARE(simulator.states()[0].position, QPointF(103, 1.5));
        // Sine: amplitude 4, period 8, phase 90 degrees: at its top at tick 0.
        simulator.runTo(0);
        QCOMPARE(simulator.sineOffset(0).y(), 4.0);
        QCOMPARE(simulator.position(0), QPointF(10, 24));
        QCOMPARE(simulator.sineOffset(2).x(), 2.0);
        simulator.runTo(4);
        QVERIFY(std::abs(simulator.sineOffset(0).y() + 4.0) < 1e-9);
        simulator.runTo(41);
        QCOMPARE(simulator.states()[1].position, QPointF(100, 50));
    }

    void animationFrames()
    {
        const AirDocument air = AirDocument::parse(QStringLiteral("[Begin Action 1]\n0,0, 0,0, 3\n0,1, 0,0, 2\n"
                                                                  "LoopStart\n0,2, 0,0, 4\n0,3, 0,0, 1\n"
                                                                  "[Begin Action 2]\n0,0, 0,0, 2\n0,1, 0,0, -1\n"));
        const AirAnimation& a = air.animations()[0];
        QCOMPARE(BgSimulator::animationFrame(a, 0), 0);
        QCOMPARE(BgSimulator::animationFrame(a, 3), 1);
        QCOMPARE(BgSimulator::animationFrame(a, 5), 2);
        QCOMPARE(BgSimulator::animationFrame(a, 9), 3);
        QCOMPARE(BgSimulator::animationFrame(a, 10), 2);
        QCOMPARE(BgSimulator::animationFrame(a, 14), 3);
        QCOMPARE(BgSimulator::animationFrame(a, 15), 2);
        const AirAnimation& endless = air.animations()[1];
        QCOMPARE(BgSimulator::animationFrame(endless, 1), 0);
        QCOMPARE(BgSimulator::animationFrame(endless, 500), 1);
    }
};

QTEST_GUILESS_MAIN(BgDocumentTest)
#include "BgDocumentTest.moc"
