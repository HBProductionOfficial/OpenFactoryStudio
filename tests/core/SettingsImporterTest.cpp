#include "core/settings/SettingsImporter.h"
#include "core/engine/RunConfiguration.h"
#include "core/settings/Config.h"
#include "core/settings/History.h"
#include "core/settings/ShortcutSettings.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace OFS;

namespace {

void writeFile(const QString& path, const QByteArray& data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

QByteArray configXml(const QList<QPair<QString, QString>>& values)
{
    QByteArray data = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<configuration>\n";
    for (const auto& [name, value] : values)
        data += "    <config name=\"" + name.toUtf8() + "\" value=\"" + value.toUtf8() + "\"/>\n";
    data += "</configuration>\n";
    return data;
}

const QList<QRect> kScreens = {QRect(0, 0, 1920, 1040), QRect(1920, 0, 1280, 984)};

} // namespace

class SettingsImporterTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void defaultSourceFolder()
    {
        QVERIFY(SettingsImporter::defaultSourceDir().endsWith(QLatin1String("/Fighter Factory Studio")));
    }

    void fitRectToScreens()
    {
        QCOMPARE(SettingsImporter::fitToScreens(QRect(100, 100, 800, 600), kScreens), QRect(100, 100, 800, 600));
        QCOMPARE(SettingsImporter::fitToScreens(QRect(2000, 10, 400, 300), kScreens), QRect(2000, 10, 400, 300));
        QCOMPARE(SettingsImporter::fitToScreens(QRect(-3000, 50, 4000, 900), kScreens), QRect(0, 50, 1920, 900));
        QCOMPARE(SettingsImporter::fitToScreens(QRect(1800, 100, 600, 400), kScreens), QRect(1920, 100, 600, 400));
        QCOMPARE(SettingsImporter::fitToScreens(QRect(-2000, -2000, 500, 500), kScreens), QRect(0, 0, 500, 500));
        QCOMPARE(SettingsImporter::fitToScreens(QPoint(5000, 100), kScreens, QSize(200, 100)), QPoint(3000, 100));
        QCOMPARE(SettingsImporter::fitToScreens(QRect(-5000, 0, 10, 10), {}), QRect(-5000, 0, 10, 10));
    }

    void importsPreviousInstallation()
    {
        QTemporaryDir dir;
        const QString source = dir.filePath("previous");
        const QString existingFolder = dir.filePath("sprites");
        QDir().mkpath(existingFolder);
        const QString missing = dir.filePath("gone/away");
        const QString charA = dir.filePath("chars/a/a.def");
        const QString charB = dir.filePath("chars/b/b.def");
        const QString engineExe = dir.filePath("mugen/mugen.exe");
        writeFile(charA, "[Info]\n");
        writeFile(charB, "[Info]\n");
        writeFile(engineExe, "MZ");

        writeFile(source + "/config.xml", configXml({{"advpal_geometry", "100,100,800,600"},
                                                     {"calculator_x", "5000"},
                                                     {"calculator_y", "100"},
                                                     {"colorset", "Dark"},
                                                     {"font_size", "12"},
                                                     {"iconset", "Missing"},
                                                     {"max_history", "5"},
                                                     {"script_geometry", "-2000,-2000,500,500"},
                                                     {"showmsg_reload_file", "1"},
                                                     {"spr_path", existingFolder},
                                                     {"sprites_path", missing},
                                                     {"background_file", missing + "/back.png"},
                                                     {"unknown_key", "kept"},
                                                     {"window_height", "900"},
                                                     {"window_left", "-3000"},
                                                     {"window_top", "50"},
                                                     {"window_width", "4000"}}));
        writeFile(source + "/shortcuts.xml", "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<shortcuts>\n"
                                             "    <shortcut action=\"file_new\" keys=\"\"/>\n"
                                             "    <shortcut action=\"file_open\" keys=\"Ctrl+Shift+O\"/>\n"
                                             "    <shortcut action=\"file_open\" keys=\"Ctrl+O\"/>\n"
                                             "    <shortcut action=\"view_zoomin\" keys=\"Ctrl++\"/>\n"
                                             "</shortcuts>\n");
        writeFile(source + "/history.xml", "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<history>\n"
                                           "    <entry name=\"A\" file=\"" +
                                               charA.toUtf8() +
                                               "\"/>\n"
                                               "    <entry name=\"Missing\" file=\"" +
                                               missing.toUtf8() +
                                               "/x.def\"/>\n"
                                               "    <entry name=\"B\" file=\"" +
                                               QDir::toNativeSeparators(charB).toUtf8() +
                                               "\"/>\n"
                                               "</history>\n");
        writeFile(source + "/mugen_config.xml", configXml({{"run_count", "1"},
                                                           {"run_0_name", "MUGEN 1.1"},
                                                           {"run_0_exe", engineExe},
                                                           {"run_0_params", "%0 kfm"},
                                                           {"p1_left", "Q"}}));

        const QString defaults = dir.filePath("defaults.xml");
        writeFile(defaults, configXml({{"font_size", "10"}, {"iconset", "Nuvola"}, {"sprites_path", ""}}));

        Config config;
        config.load(defaults, dir.filePath("user/config.xml"));
        ShortcutSettings shortcuts;
        shortcuts.load({}, dir.filePath("user/shortcuts.xml"));
        History history;
        history.load(dir.filePath("user/history.xml"));
        history.setMaxEntries(10);
        Config engine;
        engine.load({}, dir.filePath("user/mugen_config.xml"));

        SettingsImporter importer(source);
        QVERIFY(importer.hasSettings());
        importer.setScreens(kScreens);
        importer.setAvailableThemes({"Neo"}, {"Nuvola", "Classic"}, {}, {"Dark"});
        const SettingsImporter::Report report = importer.import(config, shortcuts, history, &engine);
        QVERIFY2(report.ok(), qPrintable(report.errors.join('\n')));

        QCOMPARE(config.intValue("font_size"), 12);
        QCOMPARE(config.value("unknown_key"), QStringLiteral("kept"));
        QCOMPARE(config.value("showmsg_reload_file"), QStringLiteral("1"));
        QCOMPARE(config.value("colorset"), QStringLiteral("Dark"));

        QCOMPARE(config.value("iconset"), QStringLiteral("Nuvola"));
        QVERIFY(report.skippedThemes.contains(QStringLiteral("iconset: Missing")));

        QCOMPARE(config.value("spr_path"), existingFolder);
        QCOMPARE(config.value("sprites_path"), QString());
        QVERIFY(config.value("background_file").isEmpty());
        QCOMPARE(report.droppedPaths.filter(QStringLiteral("sprites_path")).size(), 1);
        QCOMPARE(report.droppedPaths.filter(QStringLiteral("background_file")).size(), 1);

        QCOMPARE(QRect(config.intValue("window_left"), config.intValue("window_top"), config.intValue("window_width"),
                       config.intValue("window_height")),
                 QRect(0, 50, 1920, 900));
        QCOMPARE(config.intValue("calculator_x"), 3000);
        QCOMPARE(config.intValue("calculator_y"), 100);
        QCOMPARE(config.value("script_geometry"), QStringLiteral("0,0,500,500"));
        QCOMPARE(config.value("advpal_geometry"), QStringLiteral("100,100,800,600"));
        QVERIFY(report.adjustedGeometry.contains(QStringLiteral("window")));
        QVERIFY(report.adjustedGeometry.contains(QStringLiteral("calculator")));
        QVERIFY(report.adjustedGeometry.contains(QStringLiteral("script_geometry")));
        QVERIFY(!report.adjustedGeometry.contains(QStringLiteral("advpal_geometry")));

        QCOMPARE(ShortcutSettings::toText(shortcuts.keys("file_open")), QStringLiteral("Ctrl+Shift+O"));
        QVERIFY(shortcuts.hasEntry("file_new"));
        QVERIFY(shortcuts.keys("file_new").isEmpty());
        QCOMPARE(shortcuts.keys("view_zoomin"), QKeySequence(Qt::CTRL | Qt::Key_Plus));
        QCOMPARE(report.shortcuts, 3);

        QCOMPARE(history.maxEntries(), 5);
        QCOMPARE(history.entries().size(), 2);
        QCOMPARE(history.entries()[0], (History::Entry{"A", charA}));
        QCOMPARE(history.entries()[1], (History::Entry{"B", charB}));
        QVERIFY(report.droppedPaths.contains(missing + "/x.def"));

        const QList<RunConfiguration> runs = RunConfiguration::load(engine);
        QCOMPARE(runs.size(), 1);
        QCOMPARE(runs[0].name, QStringLiteral("MUGEN 1.1"));
        QCOMPARE(runs[0].executable, engineExe);
        QCOMPARE(engine.value("p1_left"), QStringLiteral("Q"));
        QCOMPARE(report.engineValues, 5);
    }

    void importsSelectedPartsOnly()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("config.xml"), configXml({{"font_size", "14"}}));
        writeFile(dir.filePath("shortcuts.xml"),
                  "<shortcuts><shortcut action=\"file_open\" keys=\"Ctrl+Alt+O\"/></shortcuts>");
        Config config;
        ShortcutSettings shortcuts;
        History history;
        SettingsImporter importer(dir.path());
        importer.setScreens({});
        const auto report = importer.import(config, shortcuts, history, nullptr, SettingsImporter::ShortcutsPart);
        QVERIFY(report.ok());
        QVERIFY(!config.contains("font_size"));
        QCOMPARE(ShortcutSettings::toText(shortcuts.keys("file_open")), QStringLiteral("Ctrl+Alt+O"));
        QCOMPARE(report.total(), 1);
    }

    void reportsUnreadableFiles()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("config.xml"), "<configuration><config name=\"a\" value=\"1\">");
        writeFile(dir.filePath("history.xml"), "<wrong/>");
        Config config;
        ShortcutSettings shortcuts;
        History history;
        SettingsImporter importer(dir.path());
        const auto report = importer.import(config, shortcuts, history, nullptr);
        QVERIFY(!report.ok());
        QCOMPARE(report.errors.size(), 2);
        QVERIFY(history.entries().isEmpty());
    }

    void missingFolderImportsNothing()
    {
        QTemporaryDir dir;
        SettingsImporter importer(dir.filePath("does-not-exist"));
        QVERIFY(!importer.hasSettings());
        Config config;
        ShortcutSettings shortcuts;
        History history;
        const auto report = importer.import(config, shortcuts, history, nullptr);
        QVERIFY(report.ok());
        QCOMPARE(report.total(), 0);
        QVERIFY(config.keys().isEmpty());
    }
};

QTEST_GUILESS_MAIN(SettingsImporterTest)
#include "SettingsImporterTest.moc"
