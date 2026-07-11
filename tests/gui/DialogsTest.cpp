#include "core/Version.h"
#include "core/settings/Config.h"
#include "core/tools/ClipboardStore.h"
#include "shell/dialogs/AboutDialog.h"
#include "shell/dialogs/CalculatorDialog.h"
#include "shell/dialogs/ClipboardContainer.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMimeData>
#include <QPushButton>
#include <QScreen>
#include <QSignalSpy>
#include <QTabWidget>
#include <QTest>
#include <QTextBrowser>

using namespace OFS;

class DialogsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void about()
    {
        AboutDialog dialog;
        QCOMPARE(dialog.windowTitle(), QStringLiteral("About..."));
        QCOMPARE(dialog.findChild<QLabel*>("name")->text(), QStringLiteral("Open Factory Studio"));
        QVERIFY(dialog.findChild<QLabel*>("name")->font().bold());
        QCOMPARE(dialog.findChild<QLabel*>("ver")->text(),
                 QStringLiteral(OFS_VERSION_STRING " (Qt %1)").arg(QString::fromLatin1(qVersion())));
        QVERIFY(!dialog.findChild<QLabel*>("icon")->pixmap().isNull());
        QCOMPARE(dialog.tabs()->count(), 2);
        QCOMPARE(dialog.tabs()->tabText(0), QStringLiteral("Legal information"));
        QCOMPARE(dialog.tabs()->tabText(1), QStringLiteral("Licenses"));
        QVERIFY(dialog.legalText()->toPlainText().contains(QStringLiteral("GPL-3.0")));
        QVERIFY(dialog.legalText()->toPlainText().contains(QStringLiteral(OFS_HOMEPAGE)));

        QVERIFY(AboutDialog::licenseText().contains(QStringLiteral("GNU GENERAL PUBLIC LICENSE")));
        const QString licenses = dialog.licensesText()->toPlainText();
        QVERIFY(licenses.contains(QStringLiteral("Lua")));
        QVERIFY(licenses.contains(QStringLiteral("Third party notices.pdf")));
        QVERIFY(licenses.contains(QStringLiteral("GNU GENERAL PUBLIC LICENSE")));

        QStringList texts;
        for (const QLabel* label : dialog.findChildren<QLabel*>())
            texts << label->text();
        texts << dialog.legalText()->toPlainText();
        QVERIFY(!texts.join('\n').contains(QStringLiteral("Fighter Factory")));

        auto* buttons = dialog.findChild<QDialogButtonBox*>();
        QVERIFY(buttons->button(QDialogButtonBox::Ok));
    }

    void calculatorLayout()
    {
        Config config;
        CalculatorDialog dialog(&config);
        QCOMPARE(dialog.windowTitle(), QStringLiteral("Calculator"));
        auto* display = dialog.findChild<QLineEdit*>("display");
        QVERIFY(display->isReadOnly());
        QCOMPARE(display->text(), QStringLiteral("0"));
        QVERIFY(display->alignment() & Qt::AlignRight);

        QStringList captions;
        for (const QPushButton* button : dialog.findChildren<QPushButton*>())
            captions << button->text();
        QCOMPARE(captions, (QStringList{"7",  "8",   "9", "/",  "%",   "sqrt", "&&", "4",     "5",    "6", "*",
                                        "**", "sin", "|", "1",  "2",   "3",    "-",  "1/x",   "cos",  "^", "0",
                                        ".",  "±",   "+", "!n", "tan", "~",    "pi", "clear", "back", "="}));
    }

    void calculatorButtonsAndKeys()
    {
        Config config;
        CalculatorDialog dialog(&config);
        auto click = [&dialog](const char* name) {
            QTest::mouseClick(dialog.findChild<QPushButton*>(name), Qt::LeftButton);
        };
        click("digit7");
        click("add");
        click("digit8");
        click("equals");
        QCOMPARE(dialog.display(), QStringLiteral("15"));
        click("sqrt");
        QCOMPARE(dialog.display(), QStringLiteral("3.87298334620742"));
        click("clear");
        QCOMPARE(dialog.display(), QStringLiteral("0"));
        click("digit5");
        click("factorial");
        QCOMPARE(dialog.display(), QStringLiteral("120"));
        click("bitNot");
        QCOMPARE(dialog.display(), QStringLiteral("-121"));

        click("clear");
        QTest::keyClicks(&dialog, "12*3");
        QTest::keyClick(&dialog, Qt::Key_Return);
        QCOMPARE(dialog.display(), QStringLiteral("36"));
        QTest::keyClicks(&dialog, "45");
        QTest::keyClick(&dialog, Qt::Key_Backspace);
        QCOMPARE(dialog.display(), QStringLiteral("4"));
        QTest::keyClick(&dialog, Qt::Key_Delete);
        QCOMPARE(dialog.display(), QStringLiteral("0"));

        QTest::keyClicks(&dialog, "42");
        QTest::keyClick(&dialog, Qt::Key_C, Qt::ControlModifier);
        QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("42"));
    }

    void calculatorPosition()
    {
        Config config;
        config.setValue("calculator_x", 120);
        config.setValue("calculator_y", 130);
        {
            CalculatorDialog dialog(&config);
            QSignalSpy visible(&dialog, &CalculatorDialog::visibilityChanged);
            dialog.show();
            QVERIFY(QTest::qWaitForWindowExposed(&dialog));
            QCOMPARE(dialog.pos(), QPoint(120, 130));
            dialog.move(140, 150);
            QTRY_COMPARE(config.intValue("calculator_x"), dialog.pos().x());
            QCOMPARE(config.intValue("calculator_y"), dialog.pos().y());
            dialog.hide();
            QCOMPARE(visible.count(), 2);
            QCOMPARE(visible.last().first().toBool(), false);
        }
        const QPoint stored(config.intValue("calculator_x"), config.intValue("calculator_y"));

        config.setValue("calculator_x", 50000);
        CalculatorDialog far(&config);
        far.show();
        QVERIFY(QTest::qWaitForWindowExposed(&far));
        QVERIFY(QGuiApplication::primaryScreen()->availableGeometry().contains(far.pos()));
        Q_UNUSED(stored);
    }

    void calculatorSingleton()
    {
        QWidget owner;
        CalculatorDialog* first = CalculatorDialog::showCalculator(&owner);
        QVERIFY(first);
        QCOMPARE(CalculatorDialog::showCalculator(&owner), first);
        QCOMPARE(CalculatorDialog::instance(), first);
        QVERIFY(first->isVisible());
    }

    void clipboardContainer()
    {
        ClipboardContainer container;
        QCOMPARE(container.windowTitle(), QStringLiteral("Clipboard"));
        QStringList captions;
        for (const char* name : {"bCut", "bCopy", "bPaste", "bRemove"})
            captions << container.findChild<QPushButton*>(name)->text();
        QCOMPARE(captions, (QStringList{"Cut", "Copy", "Paste", "Remove"}));
        QVERIFY(!container.findChild<QPushButton*>("bPaste")->isEnabled());

        container.setEditActions(nullptr, nullptr, nullptr);
        QClipboard* clipboard = QApplication::clipboard();
        clipboard->setText(QStringLiteral("hello"));
        QTest::mouseClick(container.findChild<QPushButton*>("bCopy"), Qt::LeftButton);
        QCOMPARE(container.store().count(), 1);
        QCOMPARE(container.store().name(0), QStringLiteral("Text: hello"));
        QCOMPARE(container.currentRow(), 0);
        QVERIFY(container.findChild<QPushButton*>("bPaste")->isEnabled());

        QAction cut;
        QAction copy;
        QAction paste;
        int cuts = 0;
        int pastes = 0;
        connect(&cut, &QAction::triggered, this, [&] {
            ++cuts;
            clipboard->setText(QStringLiteral("cut from the editor"));
        });
        connect(&copy, &QAction::triggered, this, [&] {
            auto* data = new QMimeData;
            data->setText(QStringLiteral("frames"));
            data->setData(QStringLiteral("application/x-ofs-frames"), QByteArray("\x01\x02\x03", 3));
            clipboard->setMimeData(data);
        });
        connect(&paste, &QAction::triggered, this, [&] { ++pastes; });
        container.setEditActions(&cut, &copy, &paste);

        container.cut();
        QCOMPARE(cuts, 1);
        QCOMPARE(container.store().count(), 2);
        QCOMPARE(container.store().name(1), QStringLiteral("Text: cut from the editor"));
        container.copy();
        QCOMPARE(container.store().count(), 3);
        QCOMPARE(container.currentRow(), 2);

        clipboard->setText(QStringLiteral("something else"));
        container.paste();
        QCOMPARE(pastes, 1);
        QCOMPARE(clipboard->text(), QStringLiteral("frames"));
        QCOMPARE(clipboard->mimeData()->data(QStringLiteral("application/x-ofs-frames")),
                 QByteArray("\x01\x02\x03", 3));

        QVERIFY(container.store().rename(0, QStringLiteral("Greeting")));
        QCOMPARE(container.store().data(container.store().index(0)).toString(), QStringLiteral("Greeting"));
        QVERIFY(container.putBack(0));
        QCOMPARE(clipboard->text(), QStringLiteral("hello"));
        QCOMPARE(pastes, 1);
        container.setCurrentRow(1);
        QTest::mouseClick(container.findChild<QPushButton*>("bRemove"), Qt::LeftButton);
        QCOMPARE(container.store().count(), 2);
        QCOMPARE(container.store().name(1), QStringLiteral("Text: frames"));
        container.clearAll();
        QCOMPARE(container.store().count(), 0);
        QVERIFY(!container.findChild<QPushButton*>("bRemove")->isEnabled());

        clipboard->clear();
        container.setEditActions(nullptr, nullptr, nullptr);
        container.copy();
        QCOMPARE(container.store().count(), 0);
    }

    void clipboardSingleton()
    {
        QWidget owner;
        ClipboardContainer* first = ClipboardContainer::showClipboard(&owner);
        QCOMPARE(ClipboardContainer::showClipboard(&owner), first);
        QCOMPARE(ClipboardContainer::instance(), first);
    }
};

QTEST_MAIN(DialogsTest)
#include "DialogsTest.moc"
