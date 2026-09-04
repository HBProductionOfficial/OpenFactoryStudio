#include "core/project/Project.h"
#include "core/sprites/SpriteDocument.h"
#include "engines/mugen/MugenEngine.h"
#include "engines/mugen/MugenHtmlExport.h"
#include "engines/mugen/dialogs/ExportHtmlDialog.h"
#include "formats/codecs/Gif.h"
#include "formats/sff/SffDocument.h"

#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QStandardItemModel>
#include <QTableView>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using namespace OFS;
using namespace OFS::Mugen;

namespace {

void writeFile(const QString& path, const QByteArray& data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

QByteArray readFile(const QString& path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

// Sprites 0,0 and 0,1 (character palette), 9000,1 (a portrait), palettes 1,1 and 1,2.
void writeSprites(const QString& path)
{
    SpriteDocument doc;
    Formats::Palette red(256, Formats::Rgba{200, 0, 0, 255});
    Formats::Palette blue(256, Formats::Rgba{0, 0, 200, 255});
    doc.palettes() << SpriteDocument::makePalette(1, 1, red) << SpriteDocument::makePalette(1, 2, blue);
    const QList<QPair<int, int>> ids = {{0, 0}, {0, 1}, {9000, 1}};
    for (const auto& id : ids) {
        SpriteEntry entry;
        entry.header.group = std::uint16_t(id.first);
        entry.header.number = std::uint16_t(id.second);
        entry.header.axisX = 4;
        entry.header.axisY = 8;
        entry.pixels = SpriteDocument::makePixels(Formats::Image::indexed(8, 8, std::vector<std::uint8_t>(64, 7)));
        entry.palette = doc.palettes()[0];
        doc.sprites() << entry;
    }
    Formats::SffDocument sff = doc.toSff();
    sff.version = Formats::SffVersion::V2;
    const auto bytes = sff.write();
    QVERIFY(bytes.ok());
    writeFile(path, QByteArray(reinterpret_cast<const char*>(bytes.value().data()), qsizetype(bytes.value().size())));
}

} // namespace

class HtmlExportTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void commands()
    {
        const QList<HtmlCommand> list =
            htmlCommands("[Remap]\nx = x\n[Command]\nname = \"QCF_x\"\ncommand = ~D, DF, F, x"
                         " ; a comment\ntime = 15\n\n[Command]\nname = \"AI_jump\"\n"
                         "command = U\n[Command]\nname = \"rain\"\ncommand = a+b\n"
                         "[Statedef -1]\n");
        QCOMPARE(list.size(), qsizetype(3));
        QCOMPARE(list[0].name, QStringLiteral("QCF_x"));
        QCOMPARE(list[0].command, QStringLiteral("~D, DF, F, x"));
        QVERIFY(!list[0].aiOnly);
        QVERIFY(list[1].aiOnly);
        QVERIFY(!list[2].aiOnly); // "rain" holds "ai" but is not named for the AI

        QCOMPARE(commandHtml("~D, DF, F, x"),
                 QStringLiteral("~<img src=\"images/d.png\" alt=\"D\"/>, <img src=\"images/df.png\" alt=\"DF\"/>, "
                                "<img src=\"images/f.png\" alt=\"F\"/>, <img src=\"images/x.png\" alt=\"x\"/>"));
        QCOMPARE(commandHtml("/$D,a"), QStringLiteral("/$<img src=\"images/d.png\" alt=\"D\"/>, "
                                                      "<img src=\"images/a.png\" alt=\"a\"/>"));
        QCOMPARE(commandHtml("B+s"), QStringLiteral("<img src=\"images/b2.png\" alt=\"B\"/>+"
                                                    "<img src=\"images/s.png\" alt=\"s\"/>"));
        QCOMPARE(commandHtml("~30a"), QStringLiteral("~30<img src=\"images/a.png\" alt=\"a\"/>"));
        QCOMPARE(commandHtml("<x>"), QStringLiteral("&lt;x&gt;"));
        QCOMPARE(commandGlyph("df").size(), QSize(24, 24));
    }

    void layouts()
    {
        MugenEngine engine;
        const QList<QPair<QString, QString>> chars = htmlLayouts({engine.dataDir()}, "char");
        QCOMPARE(chars.size(), qsizetype(1));
        QCOMPARE(chars.first().first, QStringLiteral("Default"));
        QCOMPARE(htmlLayouts({engine.dataDir()}, "stage").size(), qsizetype(1));
        QCOMPARE(htmlLayoutKind("storyboard"), QStringLiteral("scene"));
        QVERIFY(htmlLayoutKind("motif").isEmpty());
    }

    void characterPage()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("hero/hero.def"), "[Info]\nname = \"Hero\"\ndisplayname = \"The Hero\"\n"
                                                 "versiondate = 10,04,2026\nmugenversion = 1.0\nauthor = \"Me\"\n"
                                                 "[Files]\ncmd = hero.cmd\ncns = hero.cns\nanim = hero.air\n"
                                                 "sprite = hero.sff\n");
        writeFile(dir.filePath("hero/hero.cmd"), "[Command]\nname = \"FF\"\ncommand = F, F\n\n"
                                                 "[Command]\nname = \"hidden\"\ncommand = x\n");
        writeFile(dir.filePath("hero/hero.cns"), "[Data]\nlife = 1000\n");
        writeFile(dir.filePath("hero/hero.air"), "[Begin Action 0]\n0,0, 0,0, 10\n0,1, 0,0, 7\n");
        writeSprites(dir.filePath("hero/hero.sff"));
        MugenEngine engine;
        QString error;
        std::unique_ptr<Project> project(engine.openProject(dir.filePath("hero/hero.def"), &error));
        QVERIFY2(project, qPrintable(error));

        HtmlExportOptions options;
        options.layout = htmlLayouts({engine.dataDir()}, "char").first().second;
        options.email = "me@example.com";
        options.title = "Thanks <b>&</b>";
        options.text = "line 1\nline 2";
        options.boldNames = true;
        options.horizontalLines = true;
        options.commands = {"FF"};
        const QString html = dir.filePath("out/hero.html");
        QVERIFY2(exportHtml(*project, options, html, &error), qPrintable(error));

        const QString page = QString::fromUtf8(readFile(html));
        QVERIFY(page.contains("<title>The Hero</title>"));
        QVERIFY(!page.contains("!ff"));
        QVERIFY(page.contains("<td>Author: </td><td>Me</td>"));
        QVERIFY(page.contains("10,04,2026"));
        QVERIFY(page.contains("Thanks &lt;b&gt;&amp;&lt;/b&gt;"));
        QVERIFY(page.contains("line 1<br/>line 2"));
        QVERIFY(page.contains("<strong>FF</strong><br/><img src=\"images/f.png\" alt=\"F\"/>, "
                              "<img src=\"images/f.png\" alt=\"F\"/><hr/>"));
        QVERIFY(!page.contains("hidden"));
        QVERIFY(page.contains("<img src=\"images/big.png\" width=\"8\" height=\"8\"/>"));
        QVERIFY(page.contains("<img src=\"images/hero_anim_1.gif\"/>"));
        QVERIFY(page.contains("<img src=\"images/hero_anim_2.gif\"/>"));
        QVERIFY(readFile(html).contains("\r\n"));
        for (const char* file : {"big.png", "hero_anim_1.gif", "hero_anim_2.gif", "f.png", "b2.png", "a.png"})
            QVERIFY2(QFile::exists(dir.filePath(QStringLiteral("out/images/") + file)), file);
        QVERIFY(readFile(dir.filePath("out/images/hero_anim_1.gif")) !=
                readFile(dir.filePath("out/images/hero_anim_2.gif")));
    }

    void stagePage()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("st/st.def"), "[Info]\nname = \"Room\"\n[Camera]\nstartx = 0\n"
                                             "[StageInfo]\nlocalcoord = 320,240\n[BGdef]\nspr = st.sff\n"
                                             "[BG Floor]\ntype = normal\nspriteno = 0,0\nstart = 0,100\n");
        writeSprites(dir.filePath("st/st.sff"));
        MugenEngine engine;
        QString error;
        std::unique_ptr<Project> project(engine.openProject(dir.filePath("st/st.def"), &error));
        QVERIFY2(project, qPrintable(error));
        HtmlExportOptions options;
        options.layout = htmlLayouts({engine.dataDir()}, "stage").first().second;
        options.frameRate = 30;
        options.duration = 1;
        const QString html = dir.filePath("out/room.html");
        QVERIFY2(exportHtml(*project, options, html, &error), qPrintable(error));
        const QString page = QString::fromUtf8(readFile(html));
        QVERIFY(page.contains("<img src=\"images/stage.png\"/>"));
        QVERIFY(page.contains("<img src=\"images/stage.gif\"/>"));
        QImage still(dir.filePath("out/images/stage.png"));
        QCOMPARE(still.size(), QSize(320, 240));
        QCOMPARE(still.pixelColor(160, 95), QColor(200, 0, 0));
        QCOMPARE(still.pixelColor(10, 10), QColor(0, 0, 0));
        const QByteArray gif = readFile(dir.filePath("out/images/stage.gif"));
        auto decoded = Formats::readGif(
            Formats::ByteSpan(reinterpret_cast<const std::byte*>(gif.constData()), std::size_t(gif.size())));
        QVERIFY(decoded.ok());
        QCOMPARE(decoded.value().frames.size(), std::size_t(1));
    }

    void dialog()
    {
        ExportHtmlDialog chars({{"Default", "C:/layouts/char/Default"}}, false,
                               {{"FF", "F, F", false}, {"AI_x", "x", true}});
        QCOMPARE(chars.objectName(), QStringLiteral("ExpHtmlDlg"));
        auto* table = chars.findChild<QTableView*>("cmds");
        QVERIFY(table && !table->isHidden());
        const HtmlExportOptions options = chars.options();
        QCOMPARE(options.commands, QStringList{"FF"});
        QCOMPARE(options.layout, QStringLiteral("C:/layouts/char/Default"));
        QCOMPARE(options.frameRate, 60);

        ExportHtmlDialog stage({{"Default", "x"}}, true, {});
        QVERIFY(stage.findChild<QTableView*>("cmds")->isHidden());
        auto* emulator = stage.findChild<QCheckBox*>("emulator");
        QVERIFY(emulator->isChecked() && !emulator->isEnabled());
        QCOMPARE(stage.options().frameRate, 30);
        QVERIFY(stage.findChild<QWidget*>("static_res"));
    }
};

QTEST_MAIN(HtmlExportTest)
#include "HtmlExportTest.moc"
