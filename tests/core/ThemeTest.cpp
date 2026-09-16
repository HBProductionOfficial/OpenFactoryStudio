#include "core/theme/ColorSet.h"
#include "core/theme/IconSet.h"
#include "core/theme/StyleSheet.h"
#include "core/theme/ThemeManager.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QPalette>
#include <QStyle>
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

    void themeManagerFindsAndAppliesResources()
    {
        QTemporaryDir userDir;
        QTemporaryDir dataDir;
        writeText(dataDir.filePath("presets/Neo.lua"), "iconset = 'Flat'");
        writeText(userDir.filePath("presets/Mine.lua"), "iconset = 'Flat'");
        writeText(dataDir.filePath("styles/Dark/stylesheet.css"), "QWidget { border: [2]px; image: url(a.png); }");
        writeText(dataDir.filePath("colorsets/Dark.xml"),
                  "<colors><color name=\"background\" value=\"1,2,3\"/></colors>");
        writePng(dataDir.filePath("iconsets/Flat/x.png"));
        writeText(dataDir.filePath("iconsets/Flat/iconset.xml"),
                  "<iconset><icon name=\"file_new\" file=\"x.png\"/></iconset>");
        writePng(dataDir.filePath("engines/mugen/iconsets/Flat/st.png"));
        writeText(dataDir.filePath("engines/mugen/iconsets/Flat/iconset.xml"),
                  "<iconset><icon name=\"st_state\" file=\"st.png\"/></iconset>");

        ThemeManager theme;
        theme.setSearchDirs({userDir.path(), dataDir.path()});
        theme.setEngineDataDirs({dataDir.filePath("engines/mugen")});
        QCOMPARE(theme.availablePresets(), (QStringList{"Mine", "Neo"}));
        QCOMPARE(theme.availableStyles(), QStringList{"Dark"});
        QCOMPARE(theme.availableColorSets(), QStringList{"Dark"});
        QCOMPARE(theme.availableIconSets(), QStringList{"Flat"});

        theme.setScale(2.0);
        QVERIFY(theme.setStyle("Dark"));
        QVERIFY(theme.styleSheet().contains(QStringLiteral("border: 4px")));
        QVERIFY(theme.styleSheet().contains(QStringLiteral("/styles/Dark/a.png")));
        QVERIFY(theme.setColorSet("Dark"));
        QCOMPARE(theme.colorSet().color("background"), QColor(1, 2, 3));
        QVERIFY(theme.setIconSet("Flat"));
        QVERIFY(theme.iconSet().contains("file_new"));
        QVERIFY(theme.iconSet().contains("st_state"));
        QVERIFY(!theme.setIconSet("Missing"));
        QCOMPARE(theme.iconSetName(), QStringLiteral("Flat"));
    }

    void stylePalette()
    {
        QTemporaryDir dir;
        writeText(dir.filePath("styles/Night/stylesheet.css"), "QWidget { color: #cccccc; }");
        writeText(dir.filePath("styles/Night/palette.xml"),
                  "<palette><color role=\"Window\" value=\"51,51,51\"/>"
                  "<color role=\"WindowText\" value=\"204, 204, 204\"/>"
                  "<color role=\"WindowText\" group=\"disabled\" value=\"136,136,136\"/>"
                  "<color role=\"NoSuchRole\" value=\"1,2,3\"/><color role=\"Base\" value=\"oops\"/></palette>");
        writeText(dir.filePath("styles/Plain/stylesheet.css"), "QWidget { color: black; }");
        ThemeManager theme;
        theme.setSearchDirs({dir.path()});
        QVERIFY(theme.setStyle("Night"));
        const QPalette night = theme.palette();
        QCOMPARE(night.color(QPalette::Active, QPalette::Window), QColor(51, 51, 51));
        QCOMPARE(night.color(QPalette::Inactive, QPalette::WindowText), QColor(204, 204, 204));
        QCOMPARE(night.color(QPalette::Disabled, QPalette::WindowText), QColor(136, 136, 136));
        QCOMPARE(night.color(QPalette::Base), QApplication::style()->standardPalette().color(QPalette::Base));
        QVERIFY(theme.setStyle("Plain"));
        QCOMPARE(theme.palette().color(QPalette::Window),
                 QApplication::style()->standardPalette().color(QPalette::Window));
        QVERIFY(!ThemeManager::loadPalette(dir.filePath("missing.xml"), QPalette()));
    }
};

QTEST_MAIN(ThemeTest)
#include "ThemeTest.moc"
