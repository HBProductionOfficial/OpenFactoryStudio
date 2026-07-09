#include "core/actions/ActionRegistry.h"
#include "core/app/AppContext.h"
#include "core/app/Paths.h"
#include "core/engine/Engine.h"
#include "core/engine/EngineManager.h"
#include "core/preset/ControlRegistry.h"
#include "core/preset/PresetEngine.h"

#include <QTest>

using namespace OFS;

class PresetCoverageTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        m_context = new AppContext;
        m_context->loadSettings();
        m_context->initializeEngines();
        QVERIFY(m_context->engines().engine(QStringLiteral("mugen")));
    }

    void cleanupTestCase() { delete m_context; }

    void presets_data()
    {
        QTest::addColumn<QString>("name");
        for (const char* name : {"Classic", "Clean", "Neo", "Ultimate"})
            QTest::newRow(name) << QString::fromLatin1(name);
    }

    void presets()
    {
        QFETCH(QString, name);
        Preset::Model model;
        QVERIFY(Preset::PresetEngine::runFile(
            Paths::dataDir() + QStringLiteral("/presets/") + name + QStringLiteral(".lua"), model));
        Engine* mugen = m_context->engines().engine(QStringLiteral("mugen"));
        QVERIFY(Preset::PresetEngine::runFile(
            mugen->dataDir() + QStringLiteral("/presets/") + name + QStringLiteral(".lua"), model));
        QVERIFY2(model.errors.isEmpty(), qPrintable(model.errors.join(QLatin1Char('\n'))));
        QVERIFY(!model.iconset.isEmpty());
        QVERIFY(model.toolBars.contains(QStringLiteral("start")));
        QVERIFY(model.toolBars.contains(QStringLiteral("main")));

        for (const char* panel :
             {"definitions", "sprites", "animations", "commands", "states", "sounds", "backgrounds", "emulator"})
            QVERIFY2(model.panels.contains(QString::fromLatin1(panel)), panel);
        for (const char* panel : {"palettes", "thumbnails", "organizer", "controllers", "debug"})
            QVERIFY2(model.extraPanels.contains(QString::fromLatin1(panel)), panel);

        QStringList unknown;
        for (const QString& id : model.referencedNames()) {
            if (!m_context->controls().isKnown(id))
                unknown << id;
        }
        QVERIFY2(unknown.isEmpty(), qPrintable(QStringLiteral("unknown: ") + unknown.join(QStringLiteral(", "))));
    }

private:
    AppContext* m_context = nullptr;
};

QTEST_MAIN(PresetCoverageTest)
#include "PresetCoverageTest.moc"
