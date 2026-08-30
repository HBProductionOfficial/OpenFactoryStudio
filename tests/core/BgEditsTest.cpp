#include "core/bg/BgEdits.h"

#include <QTest>

using namespace OFS;

namespace {

QStringList lines(const char* text)
{
    return QString::fromLatin1(text).split(QLatin1Char('\n'));
}

QString joined(const QStringList& list)
{
    return list.join(QLatin1Char('\n'));
}

} // namespace

class BgEditsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void values()
    {
        QStringList text = lines("[BGdef]\nspr = a.sff\n\n[BG 0]\ntile  = 1, 0\nlayerno = 1 ; Front\n"
                                 "tilespacing = 0,0\n\n;----\n[BG 1]\nctrlID = 7\n");
        BgEdits::setValue(text, 3, QStringLiteral("tile"), QStringLiteral("1, 2"));
        QCOMPARE(text[4], QStringLiteral("tile = 1, 2"));
        BgEdits::setValue(text, 3, QStringLiteral("layerno"), QStringLiteral("2"));
        QCOMPARE(text[5], QStringLiteral("layerno = 2 ; Front"));
        BgEdits::setValue(text, 9, QStringLiteral("ctrlid"), QStringLiteral("7, 3"));
        QCOMPARE(text[10], QStringLiteral("ctrlID = 7, 3"));
        BgEdits::setNumbers(text, 3, QStringLiteral("start"), {10.25, -3.5}, {0, 0});
        QCOMPARE(text[7], QStringLiteral("start = 10.25, -3.5"));
        QCOMPARE(text[8], QString());
        BgEdits::setNumbers(text, 3, QStringLiteral("start"), {0, 0}, {0, 0});
        QCOMPARE(text[7], QString());
        BgEdits::setNumbers(text, 3, QStringLiteral("delta"), {0.5, 0.123456}, {1, 1});
        QCOMPARE(text[7], QStringLiteral("delta = 0.5, 0.1235"));
        BgEdits::removeKey(text, 3, QStringLiteral("delta"));
        QCOMPARE(text[7], QString());
        BgEdits::setHeader(text, 9, QStringLiteral("BG Floor"));
        QCOMPARE(text[9], QStringLiteral("[BG Floor]"));
        QStringList commented = lines("[SelectBG 2.A] ;Title background\na = 1");
        BgEdits::setHeader(commented, 0, QStringLiteral("SelectBG Renamed"));
        QCOMPARE(commented[0], QStringLiteral("[SelectBG Renamed] ;Title background"));
        QStringList crlf = {QStringLiteral("[BG 0]\r"), QStringLiteral("start = 0,0\r")};
        BgEdits::setValue(crlf, 0, QStringLiteral("start"), QStringLiteral("1, 1"));
        QCOMPARE(crlf[1], QStringLiteral("start = 1, 1\r"));
        BgEdits::setValue(crlf, 0, QStringLiteral("id"), QStringLiteral("3"));
        QCOMPARE(crlf[2], QStringLiteral("id = 3\r"));
        QCOMPARE(BgEdits::number(0.5), QStringLiteral("0.5"));
        QCOMPARE(BgEdits::number(-2), QStringLiteral("-2"));
        QCOMPARE(BgEdits::number(10.0), QStringLiteral("10"));
        QCOMPARE(BgEdits::numbers({1, 2.5, -0.00001}), QStringLiteral("1, 2.5, 0"));
    }

    void blocks()
    {
        // Comments right above a header belong to its section.
        QStringList text = lines("[BGdef]\nspr = a.sff\n\n;----\n; the sky\n[BG 0]\ntype = normal\n;window = 1\n\n"
                                 ";----\n[BG 1]\nid = 1\n");
        QCOMPARE(BgEdits::block(text, 5), std::make_pair(3, 9));
        QCOMPARE(BgEdits::block(text, 10), std::make_pair(9, 13));

        QStringList copy = text;
        const int duplicated = BgEdits::duplicate(copy, 5);
        QCOMPARE(duplicated, 11);
        QCOMPARE(copy.mid(9, 6), (QStringList{";----", "; the sky", "[BG 0]", "type = normal", ";window = 1", ""}));
        BgEdits::removeSection(copy, duplicated);
        QCOMPARE(copy, text);

        QStringList removed = text;
        BgEdits::removeSection(removed, 5);
        QCOMPARE(joined(removed), QStringLiteral("[BGdef]\nspr = a.sff\n\n;----\n[BG 1]\nid = 1\n"));

        QStringList added = text;
        const int at = BgEdits::insertSection(added, BgEdits::block(added, 5).second, {"[BG New]", "type = Normal"});
        QCOMPARE(at, 9);
        QCOMPARE(added.mid(9, 3), (QStringList{"[BG New]", "type = Normal", ""}));
        QCOMPARE(added[12], QStringLiteral(";----"));
        const int last = BgEdits::insertSection(added, int(added.size()), {"[BG End]", "type = Dummy"});
        QCOMPARE(added.mid(last - 1), (QStringList{"", "[BG End]", "type = Dummy", ""}));

        QStringList swapped = text;
        const int moved = BgEdits::swapSections(swapped, 10, 5);
        QCOMPARE(moved, 4);
        QCOMPARE(joined(swapped), QStringLiteral("[BGdef]\nspr = a.sff\n\n;----\n[BG 1]\nid = 1\n\n;----\n; the sky\n"
                                                 "[BG 0]\ntype = normal\n;window = 1\n"));
    }
};

QTEST_GUILESS_MAIN(BgEditsTest)
#include "BgEditsTest.moc"
