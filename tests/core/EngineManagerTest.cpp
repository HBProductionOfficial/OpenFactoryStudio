#include "core/engine/EngineManager.h"
#include "core/app/Paths.h"
#include "core/engine/Engine.h"

#include <QCoreApplication>
#include <QDir>
#include <QTest>

using namespace OFS;

class EngineManagerTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void loadsTheMugenPlugin()
    {
        EngineManager manager;
        manager.loadPlugins();
        QVERIFY2(manager.errors().isEmpty(), qPrintable(manager.errors().join(QLatin1Char('\n'))));

        Engine* mugen = manager.engine(QStringLiteral("mugen"));
        QVERIFY(mugen);
        QCOMPARE(mugen->displayName(), QStringLiteral("M.U.G.E.N"));
        QVERIFY(mugen->dataDir().endsWith(QStringLiteral("/data/engines/mugen")));

        QStringList versionIds;
        for (const EngineVersion& v : mugen->versions())
            versionIds << v.id;
        QCOMPARE(versionIds, (QStringList{"dosbeta", "winbeta", "1.0", "1.1", "ext"}));
    }

    void lookupIsCaseInsensitive()
    {
        EngineManager manager;
        manager.loadPlugins();
        QVERIFY(manager.engine(QStringLiteral("MUGEN")));
        QVERIFY(!manager.engine(QStringLiteral("unknown")));
    }

    void userDirHonoursEnvironment()
    {
        const QString dir = Paths::userDir();
        QVERIFY(!dir.isEmpty());
        QCOMPARE(QDir::cleanPath(qEnvironmentVariable("OFS_CONFIG_DIR")), dir);
    }
};

QTEST_GUILESS_MAIN(EngineManagerTest)
#include "EngineManagerTest.moc"
