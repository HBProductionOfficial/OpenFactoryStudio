#include "core/debug/Breakpoints.h"

#include <QSignalSpy>
#include <QTest>

using namespace OFS::Debug;

class BreakpointsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void numbering()
    {
        BreakpointList list;
        QSignalSpy changed(&list, &BreakpointList::changed);
        QVERIFY(list.toggle("C:/kfm/kfm.cns", 171));
        Breakpoint function;
        function.type = BreakpointType::Function;
        function.function = "200";
        QCOMPARE(list.add(function), 2);
        QCOMPARE(list.breakpoints().size(), 2);
        QCOMPARE(list.breakpoints()[0].number, 1);
        QCOMPARE(changed.count(), 2);

        QVERIFY(list.remove(1));
        Breakpoint frame;
        frame.type = BreakpointType::Frame;
        QCOMPARE(list.add(frame), 3);
        list.removeAll();
        QVERIFY(list.isEmpty());
        QCOMPARE(list.add(frame), 1);
    }

    void validity()
    {
        BreakpointList list;
        Breakpoint empty;
        QCOMPARE(list.add(empty), 0);
        empty.file = "a.cns";
        QCOMPARE(list.add(empty), 0);
        Breakpoint function;
        function.type = BreakpointType::Function;
        QCOMPARE(list.add(function), 0);
        Breakpoint memory;
        memory.type = BreakpointType::Memory;
        memory.address = "4938572";
        QCOMPARE(list.add(memory), 1);
        QVERIFY(list.isEmpty() == false);
    }

    void toggleAndFiles()
    {
        BreakpointList list;
        QVERIFY(list.toggle("C:\\\\kfm\\\\kfm.cns", 10));
        QVERIFY(list.at("c:/KFM/kfm.cns", 10));
        QVERIFY(!list.toggle("C:/kfm/kfm.cns", 10));
        QVERIFY(list.isEmpty());
        list.toggle("C:/kfm/kfm.cns", 3);
        list.toggle("C:/kfm/kfm.cmd", 4);
        QCOMPARE(list.inFile("C:/kfm/kfm.cns").size(), 1);
        list.renameFile("C:/kfm/kfm.cns", "C:/kfm/new.cns");
        QVERIFY(list.at("C:/kfm/new.cns", 3));
        QVERIFY(!list.at("C:/kfm/kfm.cns", 3));
    }

    void edits()
    {
        BreakpointList list;
        list.toggle("C:/kfm/kfm.cns", 5);
        Breakpoint changed = *list.find(1);
        changed.type = BreakpointType::Frame;
        changed.line = 7;
        changed.condition = "time = 0";
        changed.ignoreCount = 2;
        QVERIFY(list.update(changed));
        QCOMPARE(list.find(1)->type, BreakpointType::FileLine);
        QCOMPARE(list.find(1)->line, 7);
        QCOMPARE(list.find(1)->condition, QStringLiteral("time = 0"));
        QVERIFY(list.setEnabled(1, false));
        QVERIFY(!list.find(1)->enabled);
        QVERIFY(!list.setEnabled(9, false));
    }
};

QTEST_GUILESS_MAIN(BreakpointsTest)
#include "BreakpointsTest.moc"
