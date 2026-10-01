#include "core/app/AppContext.h"
#include "core/app/Languages.h"
#include "core/app/Paths.h"
#include "core/engine/RunConfiguration.h"
#include "core/settings/Config.h"
#include "core/settings/SettingsBundle.h"
#include "core/text/Encodings.h"
#include "core/theme/ColorRoles.h"
#include "core/theme/ColorSet.h"
#include "core/tools/Calculator.h"
#include "core/tools/ClipboardStore.h"

#include <QDir>
#include <QFile>
#include <QImage>
#include <QMimeData>
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

// Space-separated calculator keys: digits, ".", "neg", "back", "C", "pi", "=", operators and
// functions.
QString run(const QString& keys)
{
    Calculator calc;
    const QStringList tokens = keys.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (const QString& t : tokens) {
        if (t.size() == 1 && t[0].isDigit())
            calc.inputDigit(t[0].digitValue());
        else if (t == ".")
            calc.inputPoint();
        else if (t == "neg")
            calc.toggleSign();
        else if (t == "back")
            calc.backspace();
        else if (t == "C")
            calc.clear();
        else if (t == "pi")
            calc.inputPi();
        else if (t == "=")
            calc.equals();
        else if (t == "+")
            calc.applyBinary(Calculator::Binary::Add);
        else if (t == "-")
            calc.applyBinary(Calculator::Binary::Subtract);
        else if (t == "*")
            calc.applyBinary(Calculator::Binary::Multiply);
        else if (t == "/")
            calc.applyBinary(Calculator::Binary::Divide);
        else if (t == "%")
            calc.applyBinary(Calculator::Binary::Modulo);
        else if (t == "**")
            calc.applyBinary(Calculator::Binary::Power);
        else if (t == "&")
            calc.applyBinary(Calculator::Binary::BitAnd);
        else if (t == "|")
            calc.applyBinary(Calculator::Binary::BitOr);
        else if (t == "^")
            calc.applyBinary(Calculator::Binary::BitXor);
        else if (t == "sqrt")
            calc.applyUnary(Calculator::Unary::SquareRoot);
        else if (t == "sin")
            calc.applyUnary(Calculator::Unary::Sine);
        else if (t == "cos")
            calc.applyUnary(Calculator::Unary::Cosine);
        else if (t == "tan")
            calc.applyUnary(Calculator::Unary::Tangent);
        else if (t == "inv")
            calc.applyUnary(Calculator::Unary::Reciprocal);
        else if (t == "fact")
            calc.applyUnary(Calculator::Unary::Factorial);
        else if (t == "~")
            calc.applyUnary(Calculator::Unary::BitNot);
        else
            qFatal("unknown calculator key %s", qPrintable(t));
    }
    return calc.display();
}

} // namespace

class ToolsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void calculator_data()
    {
        QTest::addColumn<QString>("keys");
        QTest::addColumn<QString>("display");
        QTest::newRow("initial") << "" << "0";
        QTest::newRow("digits") << "1 2 3" << "123";
        QTest::newRow("leading zero") << "0 0 7" << "7";
        QTest::newRow("add") << "7 + 8 =" << "15";
        QTest::newRow("left to right") << "2 + 3 * 4 =" << "20";
        QTest::newRow("running total") << "2 + 3 *" << "5";
        QTest::newRow("operator replaced") << "9 + - 4 =" << "5";
        QTest::newRow("decimal") << "0 . 1 + 0 . 2 =" << "0.3";
        QTest::newRow("single point") << "1 . . 5" << "1.5";
        QTest::newRow("point first") << ". 5" << "0.5";
        QTest::newRow("divide") << "1 / 4 =" << "0.25";
        QTest::newRow("divide by zero") << "1 / 0 =" << Calculator::errorText();
        QTest::newRow("recover after error") << "1 / 0 = 4" << "4";
        QTest::newRow("modulo") << "1 7 % 5 =" << "2";
        QTest::newRow("power") << "2 ** 1 0 =" << "1024";
        QTest::newRow("and") << "1 2 & 1 0 =" << "8";
        QTest::newRow("or") << "1 2 | 3 =" << "15";
        QTest::newRow("xor") << "1 2 ^ 1 0 =" << "6";
        QTest::newRow("not") << "5 ~" << "-6";
        QTest::newRow("sqrt") << "8 1 sqrt" << "9";
        QTest::newRow("sqrt negative") << "4 neg sqrt" << Calculator::errorText();
        QTest::newRow("sin pi") << "pi sin" << "0";
        QTest::newRow("cos 0") << "0 cos" << "1";
        QTest::newRow("tan 0") << "0 tan" << "0";
        QTest::newRow("reciprocal") << "8 inv" << "0.125";
        QTest::newRow("reciprocal of zero") << "0 inv" << Calculator::errorText();
        QTest::newRow("factorial") << "5 fact" << "120";
        QTest::newRow("factorial of fraction") << "2 . 5 fact" << Calculator::errorText();
        QTest::newRow("pi") << "pi" << "3.14159265358979";
        QTest::newRow("pi as operand") << "2 * pi =" << "6.28318530717959";
        QTest::newRow("sign") << "1 2 neg" << "-12";
        QTest::newRow("sign twice") << "1 2 neg neg" << "12";
        QTest::newRow("sign of result") << "2 + 3 = neg" << "-5";
        QTest::newRow("negative operand") << "5 + 3 neg =" << "2";
        QTest::newRow("backspace") << "1 2 3 back" << "12";
        QTest::newRow("backspace to zero") << "5 back" << "0";
        QTest::newRow("backspace keeps results") << "2 + 3 = back" << "5";
        QTest::newRow("clear") << "1 + 2 C 3 =" << "3";
        QTest::newRow("unary in expression") << "1 0 + 9 sqrt =" << "13";
        QTest::newRow("equals repeats nothing") << "4 = =" << "4";
        QTest::newRow("new number after result") << "2 + 3 = 7" << "7";
        QTest::newRow("chain after result") << "2 + 3 = * 2 =" << "10";
    }

    void calculator()
    {
        QFETCH(QString, keys);
        QFETCH(QString, display);
        QCOMPARE(run(keys), display);
    }

    void calculatorFormat()
    {
        QCOMPARE(Calculator::format(-0.0), QStringLiteral("0"));
        QCOMPARE(Calculator::format(-2.5), QStringLiteral("-2.5"));
        QCOMPARE(Calculator::format(1e20), QStringLiteral("1e+20"));
    }

    void clipboardStore()
    {
        ClipboardStore store;
        QMimeData empty;
        QCOMPARE(store.add(&empty), -1);

        QMimeData text;
        text.setText(QStringLiteral("Hello world\nsecond line"));
        text.setData(QStringLiteral("application/x-ofs-test"), QByteArray("\x01\x02", 2));
        QCOMPARE(store.add(&text), 0);
        QCOMPARE(store.name(0), QStringLiteral("Text: Hello world"));

        QMimeData image;
        QImage img(32, 48, QImage::Format_ARGB32);
        img.fill(Qt::red);
        image.setImageData(img);
        QCOMPARE(store.add(&image, QStringLiteral("  Portrait  ")), 1);
        QCOMPARE(store.name(1), QStringLiteral("Portrait"));
        QCOMPARE(ClipboardStore::describe(&image), QStringLiteral("Image 32x48"));
        QCOMPARE(store.rowCount(), 2);

        text.setText(QStringLiteral("changed"));
        QCOMPARE(store.entryData(0)->text(), QStringLiteral("Hello world\nsecond line"));
        std::unique_ptr<QMimeData> copy(store.copy(0));
        QCOMPARE(copy->data(QStringLiteral("application/x-ofs-test")), QByteArray("\x01\x02", 2));
        std::unique_ptr<QMimeData> imageCopy(store.copy(1));
        QCOMPARE(qvariant_cast<QImage>(imageCopy->imageData()).size(), QSize(32, 48));

        QVERIFY(store.setData(store.index(0), QStringLiteral("Greeting")));
        QCOMPARE(store.data(store.index(0)).toString(), QStringLiteral("Greeting"));
        QVERIFY(!store.rename(0, QStringLiteral("   ")));
        QVERIFY(store.flags(store.index(0)) & Qt::ItemIsEditable);

        QVERIFY(store.remove(0));
        QCOMPARE(store.count(), 1);
        QCOMPARE(store.name(0), QStringLiteral("Portrait"));
        QVERIFY(!store.remove(5));
        store.clear();
        QCOMPARE(store.count(), 0);
        QVERIFY(store.copy(0) == nullptr);
    }

    void settingsBundleRoundTrip()
    {
        SettingsBundle bundle;
        bundle.config = {{"font_size", "12"}, {"preset", "Neo & co"}};
        bundle.shortcuts = {{"file_open", "Ctrl+O"}, {"file_new", ""}};
        bundle.engines["mugen"] = {{"p1_left", "A"}};
        bundle.colorSetName = QStringLiteral("Custom");
        bundle.colors = {XmlRecord{{"name", "keyword"}, {"value", "1,2,3"}, {"bold", "true"}}};

        QTemporaryDir dir;
        const QString path = dir.filePath("settings.ofsc");
        QVERIFY(bundle.save(path));

        SettingsBundle loaded;
        QString error;
        QVERIFY2(loaded.load(path, &error), qPrintable(error));
        QCOMPARE(loaded.config, bundle.config);
        QCOMPARE(loaded.shortcuts, bundle.shortcuts);
        QCOMPARE(loaded.engines, bundle.engines);
        QCOMPARE(loaded.colorSetName, QStringLiteral("Custom"));
        QCOMPARE(loaded.colors.size(), 1);
        QCOMPARE(loaded.colors[0].value("bold"), QStringLiteral("true"));

        QVERIFY(!loaded.parse("<configuration/>", &error));
        QVERIFY(loaded.config.isEmpty());
        QVERIFY(!loaded.load(dir.filePath("missing.ofsc")));
    }

    void runConfigurations()
    {
        Config config;
        QList<RunConfiguration> list = {{"MUGEN 1.1", "C:/mugen/mugen.exe", "", "%0 kfm"},
                                        {"MUGEN 1.0", "C:/old/mugen.exe", "C:/old/icon.png", "%0 %0"}};
        RunConfiguration::save(config, list);
        QCOMPARE(config.intValue("run_count"), 2);
        QCOMPARE(config.value("run_1_icon"), QStringLiteral("C:/old/icon.png"));
        QCOMPARE(RunConfiguration::load(config), list);

        list.removeFirst();
        RunConfiguration::save(config, list);
        QCOMPARE(RunConfiguration::load(config), list);
        QVERIFY(!config.contains("run_1_name"));
    }

    void runConfigurationFile()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("mugen_config.xml");
        Config config;
        RunConfiguration::save(
            config, {{"Desktop MUGEN", "C:\\Games\\Mugen\\mugen.exe", "", "%0 kfm"},
                     {"Desktop MUGEN stage", "C:\\Games\\Mugen\\mugen.exe", "", "kfm kfm -s %0"}});
        QVERIFY(RunConfiguration::writeEngineSettings(config, path));
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QByteArray expected =
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<configuration>\n"
            "    <engine name=\"Desktop MUGEN\">\n"
            "        <param name=\"exe\" value=\"C:\\Games\\Mugen\\mugen.exe\"/>\n"
            "        <param name=\"icon\" value=\"\"/>\n"
            "        <param name=\"params\" value=\"%0 kfm\"/>\n"
            "    </engine>\n"
            "    <engine name=\"Desktop MUGEN stage\">\n"
            "        <param name=\"exe\" value=\"C:\\Games\\Mugen\\mugen.exe\"/>\n"
            "        <param name=\"icon\" value=\"\"/>\n"
            "        <param name=\"params\" value=\"kfm kfm -s %0\"/>\n"
            "    </engine>\n"
            "</configuration>\n";
        QCOMPARE(file.readAll(), expected);
        file.close();

        Config loaded;
        loaded.setValue("clsn_attack", "#ff8000");
        RunConfiguration::loadEngineSettings(loaded, QString(), path);
        QCOMPARE(RunConfiguration::load(loaded).size(), 2);
        QCOMPARE(RunConfiguration::load(loaded).at(1).parameters, QStringLiteral("kfm kfm -s %0"));
        loaded.setValue("clsn_attack", "#ff8000");
        QVERIFY(RunConfiguration::writeEngineSettings(loaded, path));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QByteArray rewritten = file.readAll();
        QVERIFY(rewritten.contains(
            "    <config name=\"clsn_attack\" value=\"#ff8000\"/>\n    <engine name=\"Desktop MUGEN\">"));
        file.close();

        RunConfiguration::save(loaded, {});
        loaded.reset("clsn_attack");
        QVERIFY(RunConfiguration::writeEngineSettings(loaded, path));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<configuration/>\n"));
        QCOMPARE(RunConfiguration::parameterPresets().size(), 7);
    }

    void encodings()
    {
        const QList<TextEncoding>& list = TextEncoding::all();
        QCOMPARE(list.size(), 60);
        QCOMPARE(list.first().displayText(), QStringLiteral("System default - System"));
        QCOMPARE(list[1].displayText(), QStringLiteral("Baltic - windows-1257, CP1257"));
        QCOMPARE(TextEncoding::indexOf("utf-8"), 51);
        QCOMPARE(TextEncoding::indexOf("CP1252"), 45);
        QCOMPARE(list[45].codecName(), QStringLiteral("windows-1252"));
        QCOMPARE(TextEncoding::indexOf("nope"), -1);
    }

    void languages()
    {
        QTemporaryDir user;
        QTemporaryDir data;
        writeFile(data.filePath("languages/pt-br/info.xml"),
                  "<language><name value=\"Portugu\xC3\xAAs (Brasil)\"/><author value=\"Someone\"/>"
                  "<contact value=\"someone@example.com\"/></language>");
        writeFile(data.filePath("languages/broken/info.xml"), "<other/>");
        const QList<LanguageInfo> list = LanguageInfo::available({user.path(), data.path()});
        QCOMPARE(list.size(), 2);
        QCOMPARE(list[0].code, QStringLiteral("en-us"));
        QCOMPARE(list[1].code, QStringLiteral("pt-br"));
        QCOMPARE(list[1].name, QStringLiteral("Portugu\u00eas (Brasil)"));
        QCOMPARE(list[1].contact, QStringLiteral("someone@example.com"));
    }

    void colorRoles()
    {
        QCOMPARE(ColorRole::all().size(), 20);
        QCOMPARE(ColorRole::all().first().displayName(), QStringLiteral("Text background"));
        QVERIFY(ColorRole::find("comment")->text);
        QVERIFY(!ColorRole::find("attackbox")->text);
        QVERIFY(ColorRole::find("symbol") == nullptr);

        ColorSet set;
        set.set("comment", {QColor(1, 2, 3), false, true});
        const ColorSet complete = ColorRole::complete(set);
        QCOMPARE(complete.color("comment"), QColor(1, 2, 3));
        QCOMPARE(complete.color("sound_wave"), QColor(185, 175, 55));
        QCOMPARE(complete.roles().size(), 20);
    }

    void engineConfigFiles()
    {
        QTemporaryDir data;
        writeFile(data.filePath("engines/mugen/config.xml"),
                  "<configuration><config name=\"p1_left\" value=\"A\"/></configuration>");
        qputenv("OFS_DATA_DIR", data.path().toLocal8Bit());
        Paths::reset();
        QFile::remove(Paths::userDir() + "/mugen_config.xml");

        {
            AppContext context;
            Config& engine = context.engineConfig("mugen");
            QCOMPARE(&engine, &context.engineConfig("mugen"));
            QCOMPARE(engine.value("p1_left"), QStringLiteral("A"));
            engine.setValue("p1_left", QStringLiteral("Q"));
            QVERIFY(engine.save());
        }
        QVERIFY(QFile::exists(Paths::userDir() + "/mugen_config.xml"));
        {
            AppContext context;
            QCOMPARE(context.engineConfig("mugen").value("p1_left"), QStringLiteral("Q"));
        }
        QFile::remove(Paths::userDir() + "/mugen_config.xml");
        qunsetenv("OFS_DATA_DIR");
        Paths::reset();
    }
};

QTEST_MAIN(ToolsTest)
#include "ToolsTest.moc"
