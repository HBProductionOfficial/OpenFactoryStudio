#include "shell/dialogs/FirstRunDialog.h"
#include "core/app/AppContext.h"
#include "core/settings/Config.h"

#include <QAbstractButton>
#include <QFile>
#include <QTest>

using namespace OFS;

class FirstRunDialogTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void choosesAThemeAndMarksTheConfigurationInitialized()
    {
        QFile::remove(qEnvironmentVariable("OFS_CONFIG_DIR") + QStringLiteral("/config.xml"));
        AppContext context;
        context.loadSettings();
        QVERIFY(FirstRunDialog::isNeeded(context));

        QCOMPARE(FirstRunDialog::themes().size(), 6);
        FirstRunDialog dialog(context);
        QCOMPARE(dialog.selectedTheme(), 0);
        QVERIFY(dialog.findChild<QAbstractButton*>(QStringLiteral("theme5")));
        dialog.selectTheme(4);
        QCOMPARE(dialog.selectedTheme(), 4);
        dialog.applySelection();

        Config& config = context.config();
        QCOMPARE(config.value("preset"), QStringLiteral("Ultimate"));
        QCOMPARE(config.value("iconset"), QStringLiteral("Ultimate"));
        QVERIFY(config.boolValue("initialized"));
        QVERIFY(!config.boolValue("config_wizard", true));
        QVERIFY(!FirstRunDialog::isNeeded(context));

        AppContext::instance()->config().load(QString(), config.userPath());
        QVERIFY(!FirstRunDialog::isNeeded(context));
    }
};

QTEST_MAIN(FirstRunDialogTest)
#include "FirstRunDialogTest.moc"
