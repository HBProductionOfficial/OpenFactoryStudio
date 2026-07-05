#include "shell/MainWindow.h"
#include "core/engine/EngineManager.h"

#include <QTest>

using namespace OFS;

class MainWindowTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void showsApplicationTitle()
    {
        EngineManager engines;
        engines.loadPlugins();
        MainWindow window(&engines);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QCOMPARE(window.windowTitle(), QStringLiteral("Open Factory Studio"));
    }
};

QTEST_MAIN(MainWindowTest)
#include "MainWindowTest.moc"
