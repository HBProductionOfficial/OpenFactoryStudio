#include "core/theme/ColorSet.h"
#include "core/theme/IconSet.h"
#include "core/theme/StyleSheet.h"

#include <QDir>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>
#include <QTest>

using namespace OFS;

namespace {

void writeText(const QString& path, const QByteArray& data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

void writePng(const QString& path)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QImage img(4, 4, QImage::Format_ARGB32);
    img.fill(Qt::red);
    QVERIFY(img.save(path));
}

} // namespace

class ThemeTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void dpiTokens_data()
    {
        QTest::addColumn<double>("scale");
        QTest::addColumn<QString>("input");
        QTest::addColumn<QString>("expected");
        QTest::newRow("1x") << 1.0 << "border: [2]px;" << "border: 2px;";
        QTest::newRow("1.5x") << 1.5 << "border: [2]px; margin: [3]px;" << "border: 3px; margin: 5px;";
        QTest::newRow("2x") << 2.0 << "qproperty-leftMargin: [8];" << "qproperty-leftMargin: 16;";
        QTest::newRow("hairline") << 0.5 << "border: [1]px;" << "border: 1px;";
        QTest::newRow("zero") << 1.5 << "border: [0]px;" << "border: 0px;";
        QTest::newRow("negative") << 1.5 << "margin: [-2]px;" << "margin: -3px;";
        QTest::newRow("selector untouched")
            << 2.0 << "QPushButton[flat=\"true\"] { }" << "QPushButton[flat=\"true\"] { }";
    }

    void dpiTokens()
    {
        QFETCH(double, scale);
        QFETCH(QString, input);
        QFETCH(QString, expected);
        QCOMPARE(StyleSheet::preprocess(input, scale, QString()), expected);
    }

    void relativeUrls()
    {
        const QString out =
            StyleSheet::preprocess(QStringLiteral("a { border-image: url(panel.png); } b { image: url('sub/x.png'); } "
                                                  "c { image: url(:/res/y.png); } d { image: url(C:/abs/z.png); }"),
                                   1.0, QStringLiteral("D:/data/styles/Dark"));
        QVERIFY(out.contains(QStringLiteral("url(\"D:/data/styles/Dark/panel.png\")")));
        QVERIFY(out.contains(QStringLiteral("url(\"D:/data/styles/Dark/sub/x.png\")")));
        QVERIFY(out.contains(QStringLiteral("url(\":/res/y.png\")")));
        QVERIFY(out.contains(QStringLiteral("url(\"C:/abs/z.png\")")));
    }

    void colorSetRoundTrip()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("Test.xml");
        writeText(path, "<colors>\n"
                        "    <color name=\"keyword\" value=\"255,255,255\" bold=\"true\"/>\n"
                        "    <color name=\"comment\" value=\"128,128,128\" italic=\"true\"/>\n"
                        "    <color name=\"bad\" value=\"nope\"/>\n"
                        "</colors>\n");
        ColorSet set;
        QVERIFY(set.load(path));
        QCOMPARE(set.name(), QStringLiteral("Test"));
        QCOMPARE(set.color("keyword"), QColor(255, 255, 255));
        QVERIFY(set.entry("keyword").bold);
        QVERIFY(set.entry("comment").italic);
        QVERIFY(!set.contains("bad"));
        QCOMPARE(set.color("missing", Qt::green), QColor(Qt::green));

        const QString copy = dir.filePath("Copy.xml");
        QVERIFY(set.save(copy));
        ColorSet reloaded;
        QVERIFY(reloaded.load(copy));
        QVERIFY(reloaded == set);
    }

    void iconSetWithOverlay()
    {
        QTemporaryDir dir;
        const QString global = dir.filePath("iconsets/Test");
        const QString overlay = dir.filePath("engines/mugen/iconsets/Test");
        writePng(global + "/files/open.png");
        writePng(global + "/files/new.png");
        writePng(overlay + "/st/state.png");
        writeText(global + "/iconset.xml", "<!DOCTYPE VTKIconsetML>\n<iconset>\n"
                                           "    <icon name=\"file_open\" file=\"files/open.png\"/>\n"
                                           "    <icon name=\"file_new\" file=\"files/new.png\"/>\n"
                                           "</iconset>\n");
        writeText(overlay + "/iconset.xml", "<iconset>\n"
                                            "    <icon name=\"st_state\" file=\"st/state.png\"/>\n"
                                            "    <icon name=\"file_new\" file=\"*files/open.png\"/>\n"
                                            "</iconset>\n");
        IconSet set;
        QVERIFY(set.load(global));
        QVERIFY(set.addOverlay(overlay));
        QCOMPARE(set.name(), QStringLiteral("Test"));
        QVERIFY(!set.icon("file_open").isNull());
        QVERIFY(!set.icon("st_state").isNull());
        QVERIFY(set.icon("unknown").isNull());
        QCOMPARE(set.filePath("file_new"), QDir::cleanPath(global + "/files/open.png"));
        QCOMPARE(IconSet::available({dir.path()}), QStringList{"Test"});
    }
};

QTEST_MAIN(ThemeTest)
#include "ThemeTest.moc"
