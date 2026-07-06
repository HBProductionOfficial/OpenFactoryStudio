#include "core/settings/Config.h"
#include "core/settings/History.h"
#include "core/settings/ShortcutSettings.h"
#include "core/settings/XmlRecordFile.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace OFS;

namespace {

void writeFile(const QString& path, const QByteArray& data)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

QByteArray readFile(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return f.readAll();
}

} // namespace

class SettingsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void recordFileFormat()
    {
        const QByteArray data = XmlRecordFile::serialize(
            QStringLiteral("configuration"), QStringLiteral("config"),
            {XmlRecord{{"name", "a_key"}, {"value", "1"}}, XmlRecord{{"name", "b_key"}, {"value", "x & y"}}});
        QCOMPARE(data, QByteArray("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                                  "<configuration>\n"
                                  "    <config name=\"a_key\" value=\"1\"/>\n"
                                  "    <config name=\"b_key\" value=\"x &amp; y\"/>\n"
                                  "</configuration>\n"));

        const QByteArray empty =
            XmlRecordFile::serialize(QStringLiteral("configuration"), QStringLiteral("config"), {});
        QCOMPARE(empty, QByteArray("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<configuration/>\n"));
    }

    void recordFileRoundTrip()
    {
        const QList<XmlRecord> records = {XmlRecord{{"action", "file_open"}, {"keys", "Ctrl+O"}},
                                          XmlRecord{{"action", "file_new"}, {"keys", ""}}};
        const QByteArray data =
            XmlRecordFile::serialize(QStringLiteral("shortcuts"), QStringLiteral("shortcut"), records);
        const auto parsed = XmlRecordFile::parse(data, QStringLiteral("shortcuts"), QStringLiteral("shortcut"));
        QVERIFY(parsed.ok());
        QCOMPARE(parsed.records.size(), 2);
        QCOMPARE(parsed.records[0].value("action"), QStringLiteral("file_open"));
        QCOMPARE(parsed.records[1].value("keys"), QString());
        QVERIFY(parsed.records[1].has("keys"));
    }

    void recordFileRejectsWrongRoot()
    {
        const auto parsed = XmlRecordFile::parse("<other/>", QStringLiteral("configuration"), QStringLiteral("config"));
        QVERIFY(!parsed.ok());
    }

    void configDefaultsAndOverrides()
    {
        QTemporaryDir dir;
        const QString defaults = dir.filePath("defaults.xml");
        const QString user = dir.filePath("user/config.xml");
        writeFile(defaults, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<configuration>\n"
                            "    <config name=\"zoom_level\" value=\"1\"/>\n"
                            "    <config name=\"show_rulers\" value=\"false\"/>\n"
                            "    <config name=\"window_geometry\" value=\"10,20,800,600\"/>\n"
                            "</configuration>\n");

        Config config;
        config.load(defaults, user);
        QCOMPARE(config.intValue("zoom_level"), 1);
        QCOMPARE(config.boolValue("show_rulers", true), false);
        QCOMPARE(config.rectValue("window_geometry"), QRect(10, 20, 800, 600));
        QVERIFY(config.isDefault("zoom_level"));

        QSignalSpy spy(&config, &Config::changed);
        config.setValue("show_rulers", true);
        config.setValue("show_rulers", true);
        config.setValue("unknown_key", QStringLiteral("kept"));
        QCOMPARE(spy.count(), 2);
        QVERIFY(!config.isDefault("show_rulers"));
        QVERIFY(config.save());

        const QByteArray saved = readFile(user);
        QCOMPARE(saved, QByteArray("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<configuration>\n"
                                   "    <config name=\"show_rulers\" value=\"true\"/>\n"
                                   "    <config name=\"unknown_key\" value=\"kept\"/>\n"
                                   "    <config name=\"window_geometry\" value=\"10,20,800,600\"/>\n"
                                   "    <config name=\"zoom_level\" value=\"1\"/>\n"
                                   "</configuration>\n"));

        Config reloaded;
        reloaded.load(defaults, user);
        QCOMPARE(reloaded.value("unknown_key"), QStringLiteral("kept"));
        QCOMPARE(reloaded.boolValue("show_rulers"), true);

        reloaded.reset("show_rulers");
        QCOMPARE(reloaded.boolValue("show_rulers", true), false);
    }

    void configValueEncodings()
    {
        QCOMPARE(Config::encodeBool(true), QStringLiteral("true"));
        QCOMPARE(Config::decodeBool(QStringLiteral(" TRUE "), false), true);
        QCOMPARE(Config::decodeBool(QStringLiteral("garbage"), true), true);
        QCOMPARE(Config::encodeRect(QRect(-5, 3, 100, 50)), QStringLiteral("-5,3,100,50"));
        QCOMPARE(Config::decodeRect(QStringLiteral("1,2,3"), QRect(9, 9, 9, 9)), QRect(9, 9, 9, 9));
        Config config;
        config.setValue("d", 1.25);
        QCOMPARE(config.value("d"), QStringLiteral("1.25"));
        QCOMPARE(config.intValue("missing", 7), 7);
    }

    void shortcuts()
    {
        QTemporaryDir dir;
        const QString defaults = dir.filePath("shortcuts_default.xml");
        const QString user = dir.filePath("shortcuts.xml");
        writeFile(defaults, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<shortcuts>\n"
                            "    <shortcut action=\"file_open\" keys=\"Ctrl+O\"/>\n"
                            "    <shortcut action=\"view_zoomin\" keys=\"Ctrl++\"/>\n"
                            "    <shortcut action=\"code_comment\" keys=\"Ctrl+;\"/>\n"
                            "</shortcuts>\n");
        ShortcutSettings shortcuts;
        shortcuts.load(defaults, user);
        QCOMPARE(shortcuts.keys("file_open"), QKeySequence(Qt::CTRL | Qt::Key_O));
        QCOMPARE(shortcuts.keys("view_zoomin"), QKeySequence(Qt::CTRL | Qt::Key_Plus));
        QCOMPARE(shortcuts.keys("code_comment"), QKeySequence(Qt::CTRL | Qt::Key_Semicolon));
        QVERIFY(shortcuts.keys("not_there").isEmpty());

        shortcuts.setKeys("file_open", QKeySequence());
        shortcuts.setKeys("file_new", QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N));
        QVERIFY(shortcuts.save());

        ShortcutSettings reloaded;
        reloaded.load(defaults, user);
        QVERIFY(reloaded.keys("file_open").isEmpty());
        QCOMPARE(ShortcutSettings::toText(reloaded.keys("file_new")), QStringLiteral("Ctrl+Shift+N"));
        QCOMPARE(reloaded.actions().size(), 4);
    }

    void history()
    {
        QTemporaryDir dir;
        History history;
        history.load(dir.filePath("history.xml"));
        history.setMaxEntries(3);
        history.add("A", "C:/chars/a/a.def");
        history.add("B", "C:/chars/b/b.def");
        history.add("C", "C:/chars/c/c.def");
        history.add("A again", "C:/chars/a/a.def");
        history.add("D", "C:/chars/d/d.def");

        QCOMPARE(history.entries().size(), 3);
        QCOMPARE(history.entries()[0], (History::Entry{"D", "C:/chars/d/d.def"}));
        QCOMPARE(history.entries()[1], (History::Entry{"A again", "C:/chars/a/a.def"}));
        QCOMPARE(history.entries()[2], (History::Entry{"C", "C:/chars/c/c.def"}));
        QVERIFY(history.save());

        History reloaded;
        reloaded.load(dir.filePath("history.xml"));
        QCOMPARE(reloaded.entries(), history.entries());
        reloaded.remove("C:/chars/a/a.def");
        QCOMPARE(reloaded.entries().size(), 2);
    }
};

QTEST_GUILESS_MAIN(SettingsTest)
#include "SettingsTest.moc"
