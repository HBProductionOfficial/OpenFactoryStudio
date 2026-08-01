// Run with ctest -L perf. The limits allow for Debug builds on modest machines.

#include "core/app/Paths.h"
#include "core/code/CodeEditor.h"
#include "core/syntax/SyntaxDatabase.h"
#include "engines/mugen/MugenOutline.h"
#include "engines/mugen/MugenSyntax.h"

#include <QElapsedTimer>
#include <QTest>
#include <QTextBlock>

using namespace OFS;

namespace {

QString largeStateFile(int minimumLines)
{
    QStringList lines{"[Data]", "life = 1000", "attack = 100", ""};
    for (int n = 0; lines.size() < minimumLines; ++n) {
        lines << QStringLiteral("; State %1").arg(n) << QStringLiteral("[Statedef %1]").arg(n) << "type = S"
              << "movetype = A" << "physics = S" << "anim = 200" << "velset = 0,0" << ""
              << QStringLiteral("[State %1, 1]").arg(n) << "type = HitDef" << "trigger1 = AnimElem = 3 && Time > 2"
              << "attr = S, NA" << "damage = 30, 5" << "animtype = Medium" << "hitflag = MAF" << "guardflag = MA"
              << "pausetime = 12, 12" << "sparkxy = -10, -76" << "ground.velocity = -4" << "air.velocity = -3,-4"
              << "" << QStringLiteral("[State %1, 2]").arg(n) << "type = ChangeState" << "trigger1 = AnimTime = 0"
              << "value = 0" << "ctrl = 1" << "";
    }
    return lines.join(QLatin1Char('\n'));
}

} // namespace

class PerformanceTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QVERIFY2(m_db.load(Paths::dataDir() + QStringLiteral("/engines/mugen/syntax.xml")),
                 qPrintable(Paths::dataDir()));
        m_text = largeStateFile(100000);
    }

    void validation()
    {
        QElapsedTimer timer;
        timer.start();
        const IssueList issues = Mugen::validateMugenCode(m_db, QStringLiteral("state"), QStringLiteral("1.0"), m_text);
        const qint64 ms = timer.elapsed();
        qInfo("validation of 100k lines: %lld ms", ms);
        QCOMPARE(SyntaxValidator::errorCount(issues), 0);
        QVERIFY2(ms < 20000, qPrintable(QString::number(ms)));
    }

    void outline()
    {
        QElapsedTimer timer;
        timer.start();
        const Outline tree = Mugen::mugenOutline(m_text, &m_db, QStringLiteral("state"));
        const qint64 ms = timer.elapsed();
        qInfo("code tree of 100k lines: %lld ms (%lld entries)", ms, qint64(tree.size()));
        QVERIFY(tree.size() > 3000);
        QVERIFY2(ms < 20000, qPrintable(QString::number(ms)));
    }

    void loadPlain()
    {
        QPlainTextEdit editor;
        QElapsedTimer timer;
        timer.start();
        editor.setPlainText(m_text);
        QCoreApplication::processEvents();
        qInfo("loading 100k lines without highlighting: %lld ms", timer.elapsed());
    }

    void loadAndHighlight()
    {
        CodeEditor editor;
        editor.setSyntax(&m_db, QStringLiteral("state"));
        QElapsedTimer timer;
        timer.start();
        editor.setPlainText(m_text);
        QCoreApplication::processEvents();
        const qint64 ms = timer.elapsed();
        qInfo("loading and highlighting 100k lines: %lld ms", ms);
        QVERIFY(editor.blockCount() >= 100000);
        QVERIFY2(ms < 30000, qPrintable(QString::number(ms)));

        editor.goToLine(50000);
        timer.restart();
        editor.insertPlainText(QStringLiteral(";"));
        QCoreApplication::processEvents();
        const qint64 edit = timer.elapsed();
        qInfo("one edit: %lld ms", edit);
        QVERIFY2(edit < 500, qPrintable(QString::number(edit)));
    }

private:
    SyntaxDatabase m_db;
    QString m_text;
};

QTEST_MAIN(PerformanceTest)
#include "PerformanceTest.moc"
