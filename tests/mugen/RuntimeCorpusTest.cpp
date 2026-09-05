#include "engines/mugen/runtime/Character.h"
#include "engines/mugen/runtime/CharacterData.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QTest>

using namespace OFS::Mugen::Runtime;

class RuntimeCorpusTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void kungFuManMatch()
    {
        const QString engine = qEnvironmentVariable("OFS_CORPUS_MUGEN");
        const QString def = QDir(engine).filePath("chars/kfm/kfm.def");
        if (engine.isEmpty() || !QFileInfo::exists(def))
            QSKIP("OFS_CORPUS_MUGEN is not set to a M.U.G.E.N folder with chars/kfm");

        CharacterData data;
        QString error;
        QVERIFY2(data.load(def, engine, &error), qPrintable(error));
        QCOMPARE(data.name(), QStringLiteral("Kung Fu Man"));
        for (const LoadIssue& issue : data.issues())
            qWarning("%s:%d: %s", qPrintable(issue.file), issue.line + 1, qPrintable(issue.message));
        QVERIFY(data.issues().isEmpty());
        QVERIFY(data.state(0) && data.state(5000) && data.state(200));
        QVERIFY(data.animation(0));
        QVERIFY(!data.commands().isEmpty());

        Match match(1234);
        Character p1(data, &match);
        Character p2(data, &match);
        p1.setPos({-70, 0});
        p2.setPos({70, 0});
        p2.setFacing(-1);
        p1.start(0);
        p2.start(0);
        for (int t = 0; t < 3600; ++t)
            match.tick();
        QCOMPARE(match.gameTime(), 3600);
        QCOMPARE(p1.stateNo(), 0);
        QCOMPARE(p1.life(), 1000);

        QSet<QString> messages;
        for (const Character* c : {&p1, &p2}) {
            for (const QString& m : c->messages())
                messages.insert(m);
        }
        for (const QString& m : std::as_const(messages))
            qWarning("%s", qPrintable(m));
        for (const QString& m : std::as_const(messages))
            QVERIFY2(!m.startsWith("unknown controller type"), qPrintable(m));

        Match close(5);
        Character a(data, &close);
        Character b(data, &close);
        a.setPos({-20, 0});
        b.setPos({20, 0});
        b.setFacing(-1);
        a.start(200); // standing light punch
        b.start(0);
        int lifeBefore = b.life();
        for (int t = 0; t < 30; ++t)
            close.tick();
        QVERIFY2(b.life() < lifeBefore, qPrintable(QString::number(b.life())));
        QVERIFY(b.getHitVars().hitCount >= 1);

        Match grab(9);
        Character c(data, &grab);
        Character d(data, &grab);
        c.setPos({-15, 0});
        d.setPos({15, 0});
        d.setFacing(-1);
        c.start(800);
        d.start(0);
        bool held = false;
        bool thrown = false;
        int heldTicks = 0;
        for (int t = 0; t < 120 && !thrown; ++t) {
            grab.tick();
            if (d.stateNo() == 820 && d.animNo() == 820) {
                held = true;
                ++heldTicks;
                QVERIFY(d.getHitVars().fall);
            }
            thrown = d.stateNo() == 821;
        }
        QVERIFY(held);
        QVERIFY2(heldTicks > 20, qPrintable(QString::number(heldTicks)));
        QVERIFY(thrown);
        QCOMPARE(c.stateNo(), 810);
        QCOMPARE(d.life(), 1000 - 78);
        for (int t = 0; t < 120 && d.stateNo() == 821; ++t)
            grab.tick();
        QVERIFY2(d.stateNo() >= 5000 || d.stateNo() == 52, qPrintable(QString::number(d.stateNo())));
        for (int t = 0; t < 300; ++t)
            grab.tick();
        QCOMPARE(c.stateNo(), 0);
        QCOMPARE(d.stateNo(), 0);
    }
};

QTEST_GUILESS_MAIN(RuntimeCorpusTest)
#include "RuntimeCorpusTest.moc"
