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
        QStringList text = lines("[BGdef]\nspr = a.sff\n\n[BG 0]\ntype  = normal\nstart = 0, 0   ;where\n"
                                 "delta=1,1\n\n;----\n[BG 1]\nspriteno = 0,1\n");
        BgEdits::setValue(text, 3, QStringLiteral("start"), QStringLiteral("10, -5"));
        QCOMPARE(text[5], QStringLiteral("start = 10, -5   ;where"));
        BgEdits::setValue(text, 3, QStringLiteral("delta"), QStringLiteral("0.5, 1"));
        QCOMPARE(text[6], QStringLiteral("delta=0.5, 1"));
        BgEdits::setValue(text, 3, QStringLiteral("Type"), QStringLiteral("parallax"));
        QCOMPARE(text[4], QStringLiteral("type  = parallax"));
        BgEdits::setValue(text, 3, QStringLiteral("mask"), QStringLiteral("1"));
        QCOMPARE(text[7], QStringLiteral("mask = 1"));
        QCOMPARE(text[8], QString());
        BgEdits::removeKey(text, 3, QStringLiteral("mask"));
        QCOMPARE(text[7], QString());
        BgEdits::setHeader(text, 9, QStringLiteral("BG Floor"));
        QCOMPARE(text[9], QStringLiteral("[BG Floor]"));
        QStringList commented = lines("[BG 2] ;sky\na = 1");
        BgEdits::setHeader(commented, 0, QStringLiteral("BG Sky"));
        QCOMPARE(commented[0], QStringLiteral("[BG Sky] ;sky"));
        QStringList crlf = {QStringLiteral("[BG 0]\r"), QStringLiteral("start = 0,0\r")};
        BgEdits::setValue(crlf, 0, QStringLiteral("start"), QStringLiteral("1, 1"));
        QCOMPARE(crlf[1], QStringLiteral("start = 1, 1\r"));
        BgEdits::setValue(crlf, 0, QStringLiteral("id"), QStringLiteral("3"));
        QCOMPARE(crlf[2], QStringLiteral("id = 3\r"));
        QCOMPARE(BgEdits::number(0.5), QStringLiteral("0.5"));
        QCOMPARE(BgEdits::number(-2), QStringLiteral("-2"));
        QCOMPARE(BgEdits::number(1.0 / 3), QStringLiteral("0.333333"));
    }

    void sections()
    {
        QStringList text = lines("[BGdef]\nspr = a.sff\n\n[BG 0]\ntype = normal\n\n;----\n[BG 1]\nid = 1\n");
        QCOMPARE(BgEdits::sectionText(text, 3), (QStringList{"[BG 0]", "type = normal"}));
        const int at = BgEdits::insertSection(text, 3, {"[BG New]", "type = dummy"});
        QCOMPARE(at, 6);
        QCOMPARE(joined(text), QStringLiteral("[BGdef]\nspr = a.sff\n\n[BG 0]\ntype = normal\n\n[BG New]\n"
                                              "type = dummy\n\n;----\n[BG 1]\nid = 1\n"));
        const int last = BgEdits::insertSection(text, -1, {"[BG End]"});
        QCOMPARE(text[last], QStringLiteral("[BG End]"));
        BgEdits::removeSection(text, 6);
        QVERIFY(
            joined(text).startsWith(QStringLiteral("[BGdef]\nspr = a.sff\n\n[BG 0]\ntype = normal\n\n;----\n[BG 1]")));
    }
};

QTEST_GUILESS_MAIN(BgEditsTest)
#include "BgEditsTest.moc"
