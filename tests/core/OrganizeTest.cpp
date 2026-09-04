#include "core/editors/Organize.h"

#include <QTest>

using namespace OFS;

class OrganizeTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void deltas()
    {
        QCOMPARE(Organize::deltaOf("org_up", 9), -1);
        QCOMPARE(Organize::deltaOf("org_down_10", 9), 10);
        QCOMPARE(Organize::deltaOf("org_top", 9), -9);
        QCOMPARE(Organize::deltaOf("org_bottom", 9), 9);
        QCOMPARE(Organize::deltaOf("org_palette", 9), 0);
    }

    void moveOrder()
    {
        QList<int> moved;
        QCOMPARE(Organize::moveOrder(5, {2}, -1, &moved), (QList<int>{0, 2, 1, 3, 4}));
        QCOMPARE(moved, QList<int>{1});
        QCOMPARE(Organize::moveOrder(5, {1, 3}, -10, &moved), (QList<int>{1, 3, 0, 2, 4}));
        QCOMPARE(moved, (QList<int>{0, 1}));
        QCOMPARE(Organize::moveOrder(5, {0, 3}, 5, &moved), (QList<int>{1, 2, 4, 0, 3}));
        QCOMPARE(moved, (QList<int>{3, 4}));
        QCOMPARE(Organize::moveOrder(3, {0}, -1, &moved), (QList<int>{0, 1, 2}));
        QCOMPARE(moved, QList<int>{0});
    }

    void dropOrder()
    {
        QList<int> moved;
        QCOMPARE(Organize::dropOrder(5, {3}, 1, &moved), (QList<int>{0, 3, 1, 2, 4}));
        QCOMPARE(moved, QList<int>{1});
        QCOMPARE(Organize::dropOrder(5, {0, 2}, 5, &moved), (QList<int>{1, 3, 4, 0, 2}));
        QCOMPARE(moved, (QList<int>{3, 4}));
        QCOMPARE(Organize::dropOrder(5, {1, 4}, 0, &moved), (QList<int>{1, 4, 0, 2, 3}));
        QCOMPARE(Organize::dropOrder(3, {1}, 2, &moved), (QList<int>{0, 1, 2}));
        QCOMPARE(moved, QList<int>{1});
    }

    void blocks()
    {
        const QStringList lines = {
            "; Animations",      // 0 file comment (the first action's comments run up to it)
            "",                  // 1
            "; Standing",        // 2
            "[Begin Action 0]",  // 3
            "0,0, 0,0, 10",      // 4
            ";0,1, 0,0, 10",     // 5 a commented frame stays with its action
            "",                  // 6
            "",                  // 7
            "; Walking",         // 8
            "[Begin Action 20]", // 9
            "20,0, 0,0, 5",      // 10
            "",                  // 11
            "[Begin Action 5]",  // 12
            "5,0, 0,0, 5",       // 13
        };
        static const QRegularExpression header("^\\s*\\[\\s*begin\\s+action\\b",
                                               QRegularExpression::CaseInsensitiveOption);
        const QList<Organize::Block> blocks = Organize::sectionBlocks(lines, header);
        QCOMPARE(blocks.size(), qsizetype(3));
        QCOMPARE(blocks[0].start, 2);
        QCOMPARE(blocks[0].contentEnd, 6);
        QCOMPARE(blocks[0].end, 8);
        QCOMPARE(blocks[1].start, 8);
        QCOMPARE(blocks[1].contentEnd, 11);
        QCOMPARE(blocks[2].start, 12);
        QCOMPARE(blocks[2].end, 14);

        QStringList moved = lines;
        Organize::reorderBlocks(moved, blocks, {2, 0, 1});
        QCOMPARE(moved, (QStringList{"; Animations", "", "[Begin Action 5]", "5,0, 0,0, 5", "", "", "; Standing",
                                     "[Begin Action 0]", "0,0, 0,0, 10", ";0,1, 0,0, 10", "", "; Walking",
                                     "[Begin Action 20]", "20,0, 0,0, 5"}));
        Organize::reorderBlocks(moved, Organize::sectionBlocks(moved, header), {1, 2, 0});
        QCOMPARE(moved, lines);

        const QStringList commands = {"[Remap]",      "x = x", "", "[Command]", "name = \"a\"", "",
                                      "[Defaults]",   "t = 1", "", "[Command]", "name = \"b\"", "",
                                      "[Statedef -1]"};
        static const QRegularExpression command("^\\s*\\[\\s*command\\s*\\]",
                                                QRegularExpression::CaseInsensitiveOption);
        QStringList swapped = commands;
        Organize::reorderBlocks(swapped, Organize::sectionBlocks(commands, command), {1, 0});
        QCOMPARE(swapped, (QStringList{"[Remap]", "x = x", "", "[Command]", "name = \"b\"", "", "[Defaults]", "t = 1",
                                       "", "[Command]", "name = \"a\"", "", "[Statedef -1]"}));
    }
};

QTEST_GUILESS_MAIN(OrganizeTest)
#include "OrganizeTest.moc"
