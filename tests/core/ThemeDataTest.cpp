#include "core/theme/ColorSet.h"
#include "core/theme/IconSet.h"
#include "core/theme/StyleSheet.h"
#include "core/theme/ThemeManager.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMainWindow>
#include <QPalette>
#include <QRegularExpression>
#include <QSpinBox>
#include <QTabBar>
#include <QTest>
#include <QTreeWidget>
#include <QVBoxLayout>

using namespace OFS;

namespace {

const QString kDataDir = QStringLiteral(OFS_SOURCE_DATA_DIR);

QStringList jsonList(const QJsonObject& obj, const char* key)
{
    QStringList out;
    for (const QJsonValue& v : obj.value(QLatin1String(key)).toArray())
        out << v.toString();
    return out;
}

QJsonObject requiredNames()
{
    QFile f(QStringLiteral(OFS_ICONSET_NAMES));
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}

QStringList g_warnings;
void collectWarnings(QtMsgType type, const QMessageLogContext&, const QString& msg)
{
    // Only style sheet problems count. The offscreen platform has unrelated warnings of its own.
    static const QRegularExpression styleIssue(QStringLiteral("style ?sheet|parse|property|qss"),
                                               QRegularExpression::CaseInsensitiveOption);
    if ((type == QtWarningMsg || type == QtCriticalMsg) && styleIssue.match(msg).hasMatch())
        g_warnings << msg;
}

} // namespace

// Stand-in with the class name and properties the style sheets set on side panels.
class EPanelWidget : public QFrame
{
    Q_OBJECT
    Q_PROPERTY(int leftMargin MEMBER m_left)
    Q_PROPERTY(int topMargin MEMBER m_top)
    Q_PROPERTY(int rightMargin MEMBER m_right)
    Q_PROPERTY(int bottomMargin MEMBER m_bottom)

public:
    using QFrame::QFrame;

private:
    int m_left = 0;
    int m_top = 0;
    int m_right = 0;
    int m_bottom = 0;
};

class ThemeDataTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void svgSupportAvailable()
    {
        QVERIFY2(QImageReader::supportedImageFormats().contains("svg"), "Qt SVG image format plugin not found");
    }

    void iconSets_data()
    {
        QTest::addColumn<QString>("name");
        for (const char* set : {"Nuvola", "Flat-Dark", "Flat-Light", "Classic", "Ultimate"})
            QTest::newRow(set) << QString::fromLatin1(set);
    }

    void iconSets()
    {
        QFETCH(QString, name);
        const QJsonObject required = requiredNames();
        const QStringList globalNames = jsonList(required, "global");
        const QStringList engineNames = jsonList(required, "mugen");
        QCOMPARE(globalNames.size(), 236);
        QCOMPARE(engineNames.size(), 83);

        IconSet global;
        QString error;
        QVERIFY2(global.load(kDataDir + QStringLiteral("/iconsets/") + name, &error), qPrintable(error));
        QStringList missing;
        for (const QString& icon : globalNames) {
            if (!global.contains(icon))
                missing << icon;
        }
        QVERIFY2(missing.isEmpty(), qPrintable(QStringLiteral("missing global icons: ") + missing.join(u' ')));

        IconSet set;
        QVERIFY(set.load(kDataDir + QStringLiteral("/iconsets/") + name));
        QVERIFY2(set.addOverlay(kDataDir + QStringLiteral("/engines/mugen/iconsets/") + name, &error),
                 qPrintable(error));
        for (const QString& icon : engineNames) {
            if (!set.contains(icon))
                missing << icon;
        }
        QVERIFY2(missing.isEmpty(), qPrintable(QStringLiteral("missing engine icons: ") + missing.join(u' ')));

        QStringList broken;
        for (const QString& icon : globalNames + engineNames) {
            const QString file = set.filePath(icon);
            if (!QFileInfo::exists(file)) {
                broken << icon + QStringLiteral(" (no file ") + file + u')';
                continue;
            }
            for (int size : {16, 24, 32}) {
                if (set.icon(icon).pixmap(size, size).isNull()) {
                    broken << icon + QStringLiteral(" at ") + QString::number(size);
                    break;
                }
            }
        }
        QVERIFY2(broken.isEmpty(), qPrintable(QStringLiteral("unusable icons: ") + broken.join(QStringLiteral(", "))));
    }

    void iconsAreNotBlank()
    {
        for (const char* setName : {"Nuvola", "Flat-Dark", "Flat-Light"}) {
            IconSet set;
            QVERIFY(set.load(kDataDir + QStringLiteral("/iconsets/") + QLatin1String(setName)));
            set.addOverlay(kDataDir + QStringLiteral("/engines/mugen/iconsets/") + QLatin1String(setName));
            QStringList blank;
            for (const QString& icon : set.names()) {
                const QImage img = set.icon(icon).pixmap(32, 32).toImage().convertToFormat(QImage::Format_ARGB32);
                int visible = 0;
                for (int y = 0; y < img.height(); ++y) {
                    const QRgb* line = reinterpret_cast<const QRgb*>(img.constScanLine(y));
                    for (int x = 0; x < img.width(); ++x)
                        visible += qAlpha(line[x]) > 32 ? 1 : 0;
                }
                if (visible < 20)
                    blank << icon;
            }
            QVERIFY2(blank.isEmpty(),
                     qPrintable(QLatin1String(setName) + QStringLiteral(": blank icons: ") + blank.join(u' ')));
        }
    }

    void themeManagerListsShippedThemes()
    {
        ThemeManager theme;
        theme.setSearchDirs({kDataDir});
        theme.setEngineDataDirs({kDataDir + QStringLiteral("/engines/mugen")});
        QCOMPARE(theme.availableIconSets(), (QStringList{"Classic", "Flat-Dark", "Flat-Light", "Nuvola", "Ultimate"}));
        QCOMPARE(theme.availableStyles(), (QStringList{"Classic", "Dark", "Light", "Ultimate"}));
        QCOMPARE(theme.availableColorSets(),
                 (QStringList{"An Old Hope", "Classic", "Dark", "Light", "Monokai", "Ultimate"}));
        QVERIFY(theme.setIconSet(QStringLiteral("Nuvola")));
        QVERIFY(theme.iconSet().contains(QStringLiteral("st_a")));
        QVERIFY(theme.iconSet().contains(QStringLiteral("file_new")));
    }

    void colorSets_data()
    {
        QTest::addColumn<QString>("name");
        for (const char* set : {"An Old Hope", "Classic", "Dark", "Light", "Monokai", "Ultimate"})
            QTest::newRow(set) << QString::fromLatin1(set);
    }

    void colorSets()
    {
        QFETCH(QString, name);
        static const QStringList roles = {
            "back_start",  "back_end", "background",    "foreground",  "attackbox", "collisionbox", "sound_wave",
            "block_range", "line",     "paren",         "paren_range", "comment",   "block",        "number",
            "string",      "keyword",  "keyword_value", "block_type",  "function",  "const",        "symbol"};
        ColorSet set;
        QString error;
        QVERIFY2(set.load(kDataDir + QStringLiteral("/colorsets/") + name + QStringLiteral(".xml"), &error),
                 qPrintable(error));
        for (const QString& role : roles)
            QVERIFY2(set.contains(role), qPrintable(name + QStringLiteral(": missing role ") + role));
        QCOMPARE(set.roles().size(), roles.size());
        QCOMPARE(set.color(QStringLiteral("attackbox")), QColor(255, 0, 0));
        QCOMPARE(set.color(QStringLiteral("collisionbox")), QColor(0, 0, 255));
    }

    void darkColorSetValues()
    {
        ColorSet set;
        QVERIFY(set.load(kDataDir + QStringLiteral("/colorsets/Dark.xml")));
        QCOMPARE(set.color(QStringLiteral("background")), QColor(34, 34, 34));
        QCOMPARE(set.color(QStringLiteral("back_start")), QColor(85, 85, 85));
        QVERIFY(set.entry(QStringLiteral("keyword")).bold);
        QVERIFY(set.entry(QStringLiteral("comment")).italic);
        QVERIFY(!set.entry(QStringLiteral("string")).bold);
    }

    void styleSheets_data()
    {
        QTest::addColumn<QString>("name");
        QTest::addColumn<double>("scale");
        for (const char* style : {"Dark", "Light", "Classic", "Ultimate"}) {
            QTest::addRow("%s 1x", style) << QString::fromLatin1(style) << 1.0;
            QTest::addRow("%s 1.5x", style) << QString::fromLatin1(style) << 1.5;
        }
    }

    void styleSheets()
    {
        QFETCH(QString, name);
        QFETCH(double, scale);
        const QString dir = kDataDir + QStringLiteral("/styles/") + name;
        const QString sheet = StyleSheet::load(dir, scale);
        QVERIFY(!sheet.isEmpty());
        static const QRegularExpression token(QStringLiteral("\\[\\s*-?\\d+(\\.\\d+)?\\s*\\]"));
        QVERIFY2(!token.match(sheet).hasMatch(), "unexpanded [N] token left in the style sheet");
        QVERIFY(sheet.contains(QStringLiteral("EPanelWidget")));
        QVERIFY(sheet.contains(QStringLiteral("EImageWidget")));

        static const QRegularExpression url(QStringLiteral("url\\(\"([^\"]+)\"\\)"));
        for (auto it = url.globalMatch(sheet); it.hasNext();) {
            const QString file = it.next().captured(1);
            QVERIFY2(QFileInfo::exists(file), qPrintable(QStringLiteral("missing style image ") + file));
            QVERIFY2(!QImageReader(file).read().isNull(), qPrintable(QStringLiteral("unreadable image ") + file));
        }

        g_warnings.clear();
        const QtMessageHandler previous = qInstallMessageHandler(collectWarnings);
        {
            QMainWindow win;
            auto* central = new QWidget;
            auto* layout = new QVBoxLayout(central);
            auto* panel = new EPanelWidget;
            auto* panelLayout = new QVBoxLayout(panel);
            auto* title = new QLabel(QStringLiteral("Definitions"));
            title->setObjectName(QStringLiteral("panelTitle"));
            panelLayout->addWidget(title);
            panelLayout->addWidget(new QCheckBox(QStringLiteral("Decompress on load")));
            layout->addWidget(panel);
            layout->addWidget(new QTreeWidget);
            layout->addWidget(new QSpinBox);
            layout->addWidget(new QComboBox);
            layout->addWidget(new QCheckBox(QStringLiteral("check")));
            auto* tabs = new QTabBar;
            tabs->addTab(QStringLiteral("kfm.def"));
            tabs->setTabsClosable(true);
            layout->addWidget(tabs);
            win.setCentralWidget(central);
            win.setStyleSheet(sheet);
            win.resize(400, 300);
            win.show();
            QCoreApplication::processEvents();
            win.grab();
        }
        qInstallMessageHandler(previous);
        QVERIFY2(g_warnings.isEmpty(), qPrintable(g_warnings.join(u'\n')));
    }

    void shippedStylePalettes()
    {
        const QString data = QStringLiteral(OFS_SOURCE_DATA_DIR);
        for (const char* name : {"Dark", "Light"}) {
            const auto palette =
                ThemeManager::loadPalette(data + "/styles/" + QLatin1String(name) + "/palette.xml", QPalette());
            QVERIFY2(palette, name);
            const bool dark = QLatin1String(name) == QLatin1String("Dark");
            QCOMPARE(palette->color(QPalette::Window).lightness() < 128, dark);
            QCOMPARE(palette->color(QPalette::Base).lightness() < 128, dark);
            QCOMPARE(palette->color(QPalette::WindowText).lightness() < 128, !dark);
            QCOMPARE(palette->color(QPalette::Text).lightness() < 128, !dark);
        }
    }
};

QTEST_MAIN(ThemeDataTest)
#include "ThemeDataTest.moc"
