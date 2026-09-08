// Debug > Add breakpoint... ("BreakpointDlg").
#include "shell/dialogs/BreakpointDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTest>

using namespace OFS;
using Debug::Breakpoint;
using Debug::BreakpointType;

class BreakpointDialogTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void fields()
    {
        BreakpointDialog dialog;
        QCOMPARE(dialog.objectName(), QStringLiteral("BreakpointDlg"));
        QCOMPARE(dialog.windowTitle(), QStringLiteral("Breakpoint"));
        auto* type = dialog.findChild<QComboBox*>("brkType");
        QStringList types;
        for (int i = 0; i < type->count(); ++i)
            types << type->itemText(i);
        QCOMPARE(types, (QStringList{"File Name and Line Number", "Function Name", "Break on Memory Access",
                                     "Break on Frame"}));
        for (const char* name : {"type_lb", "file_lb", "line_lb", "enabled_lb", "address_lb", "function_lb",
                                 "condition_lb", "ignore_lb", "one_shot_lb"})
            QVERIFY2(dialog.findChild<QWidget*>(name), name);
        auto* file = dialog.findChild<QLineEdit*>("brkFile");
        auto* browse = dialog.findChild<QPushButton*>("brkBrowse");
        auto* line = dialog.findChild<QSpinBox*>("brkLine");
        auto* address = dialog.findChild<QLineEdit*>("brkAddress");
        auto* function = dialog.findChild<QLineEdit*>("brkFunction");
        auto* ok = dialog.findChild<QDialogButtonBox*>("buttonBox")->button(QDialogButtonBox::Ok);
        QCOMPARE(line->maximum(), 9999999);
        QCOMPARE(dialog.findChild<QSpinBox*>("brkIgnore")->maximum(), 65535);
        QVERIFY(dialog.findChild<QCheckBox*>("brkEnabled")->isChecked());
        QCOMPARE(address->text(), QStringLiteral("0"));

        QVERIFY(file->isEnabled() && browse->isEnabled() && line->isEnabled());
        QVERIFY(!address->isEnabled() && !function->isEnabled());
        QVERIFY(!ok->isEnabled());
        file->setText("C:/kfm/kfm.cns");
        line->setValue(171);
        QVERIFY(ok->isEnabled());
        type->setCurrentIndex(1);
        QVERIFY(!file->isEnabled() && !browse->isEnabled() && function->isEnabled());
        QVERIFY(!ok->isEnabled());
        function->setText("200");
        QVERIFY(ok->isEnabled());
        type->setCurrentIndex(2);
        QVERIFY(address->isEnabled());
        QVERIFY(address->text().isEmpty());
        type->setCurrentIndex(3);
        QVERIFY(ok->isEnabled());
        dialog.findChild<QLineEdit*>("brkCondition")->setText("time = 0");
        dialog.findChild<QCheckBox*>("brkOneShot")->setChecked(true);
        const Breakpoint b = dialog.breakpoint();
        QCOMPARE(b.type, BreakpointType::Frame);
        QCOMPARE(b.condition, QStringLiteral("time = 0"));
        QVERIFY(b.oneShot);
        QVERIFY(b.file.isEmpty());
    }

    void editing()
    {
        Breakpoint b;
        b.number = 4;
        b.file = "C:/kfm/kfm.cns";
        b.line = 12;
        b.ignoreCount = 2;
        b.enabled = false;
        BreakpointDialog dialog;
        dialog.setBreakpoint(b, true);
        QVERIFY(!dialog.findChild<QComboBox*>("brkType")->isEnabled());
        QCOMPARE(dialog.findChild<QSpinBox*>("brkLine")->value(), 12);
        QVERIFY(!dialog.findChild<QCheckBox*>("brkEnabled")->isChecked());
        const Breakpoint back = dialog.breakpoint();
        QCOMPARE(back.number, 4);
        QCOMPARE(back.ignoreCount, 2);
        QCOMPARE(back.file, QStringLiteral("C:/kfm/kfm.cns"));
    }
};

QTEST_MAIN(BreakpointDialogTest)
#include "BreakpointDialogTest.moc"
