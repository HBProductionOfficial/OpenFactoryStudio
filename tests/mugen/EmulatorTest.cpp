#include "core/app/AppContext.h"
#include "core/debug/Breakpoints.h"
#include "core/project/Project.h"
#include "core/project/ProjectFile.h"
#include "core/settings/Config.h"
#include "core/sprites/SpriteEdits.h"
#include "core/widgets/RenderSurface.h"
#include "engines/mugen/MugenEngine.h"
#include "engines/mugen/emulator/EmulatorRenderer.h"
#include "engines/mugen/emulator/EmulatorSession.h"
#include "engines/mugen/emulator/EmulatorWidget.h"
#include "fixtures/FixtureFactory.h"
#include "formats/codecs/Pcx.h"
#include "formats/palette/PaletteFormats.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QTextCursor>
#include <QTextDocument>
#include <QTimer>
#include <QWheelEvent>

#include <cmath>
#include <functional>
#include <memory>
#include <tuple>

using namespace OFS;
using namespace OFS::Mugen;

namespace {

void writeFile(const QString& path, const QByteArray& data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(data);
}

// Common states that start the round, a fight.def, a mugen.cfg, a stage and a test character.
struct EngineFolder
{
    QTemporaryDir dir;
    QString def;

    EngineFolder()
    {
        writeFile(dir.filePath("data/common1.cns"),
                  "[Statedef 5900]\ntype = S\n[State 5900]\ntype = ChangeState\ntrigger1 = 1\nvalue = 0\n"
                  "[Statedef 0]\ntype = S\nphysics = S\nanim = 0\nctrl = 1\n[State 0]\ntype = Null\ntrigger1 = 1\n");
        writeFile(dir.filePath("data/fight.def"), "[Round]\nmatch.wins = 1\nstart.waittime = 3\nctrl.time = 3\n");
        writeFile(dir.filePath("data/mugen.cfg"), "[Options]\ntime = 5\n");
        writeFile(dir.filePath("stages/stage0.def"),
                  "[StageInfo]\nzoffset = 200\n[Camera]\nboundleft = -100\nboundright = 100\n"
                  "[PlayerInfo]\np1startx = -50\np2startx = 50\n[BGdef]\nspr = stage0.sff\n");
        QDir().mkpath(dir.filePath("chars/test"));
        Formats::Test::FixtureFactory fixtures;
        Formats::SffDocument sff = fixtures.sff(Formats::SffVersion::V1, 2, 24);
        Formats::SffSprite& sprite = sff.sprites[0];
        sprite.axisX = std::int16_t(sprite.width / 2);
        sprite.axisY = std::int16_t(sprite.height);
        QVERIFY(sff.save(dir.filePath("chars/test/test.sff").toStdWString()).ok());
        QVERIFY(fixtures.sff(Formats::SffVersion::V1, 1).save(dir.filePath("stages/stage0.sff").toStdWString()).ok());
        writeFile(dir.filePath("chars/test/test.air"), "[Begin Action 0]\n0,0, 0,0, -1\n");
        writeFile(dir.filePath("chars/test/test.cns"), "[Data]\nlife = 1000\n");
        writeFile(dir.filePath("chars/test/test.cmd"), "[Statedef -1]\n");
        def = dir.filePath("chars/test/test.def");
        writeFile(def, "[Info]\nname = \"Tester\"\n[Files]\nsprite = test.sff\nanim = test.air\ncns = test.cns\n"
                       "st = test.cns\ncmd = test.cmd\nstcommon = common1.cns\n");
    }
};

// Sounds as (group, number, level), each holding one constant level.
void writeSounds(const QString& path, const QList<std::tuple<int, int, double>>& sounds)
{
    Formats::SndDocument snd;
    for (const auto& [group, number, value] : sounds) {
        auto wav = Formats::WavFile::fromSamples(std::vector<double>(2000, value), 1, 22050, 16, false);
        QVERIFY(wav);
        Formats::SndSound sound;
        sound.group = group;
        sound.number = number;
        sound.data = wav->write();
        snd.sounds.push_back(std::move(sound));
    }
    QVERIFY(snd.save(path.toStdWString()).ok());
}

// Line numbers on the right.
const char* kDebugCmd = "[Statedef -1]\n"           // 1
                        "[State -1, count]\n"       // 2
                        "type = VarAdd\n"           // 3
                        "trigger1 = 1\n"            // 4
                        "v = 0\n"                   // 5
                        "value = 1\n"               // 6
                        "[State -1, other]\n"       // 7
                        "type = VarAdd\n"           // 8
                        "triggerall = var(0) > 0\n" // 9
                        "trigger1 = 1\n"            // 10
                        "v = 1\n"                   // 11
                        "value = 1\n";              // 12

// Runs `run` while answering each stop of the debugger with the next step of `script`.
void whileStopped(EmulatorWidget& emulator, QList<std::function<void()>> script, const std::function<void()>& run,
                  int* answered = nullptr)
{
    int next = 0;
    QTimer driver;
    driver.setInterval(0);
    QObject::connect(&driver, &QTimer::timeout, [&] {
        if (next < script.size() && emulator.debugState() == Debug::SessionState::Interrupted)
            script[next++]();
    });
    driver.start();
    run();
    driver.stop();
    if (answered)
        *answered = next;
}

int litPixels(const QImage& image)
{
    int count = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (qRed(image.pixel(x, y)) || qGreen(image.pixel(x, y)) || qBlue(image.pixel(x, y)))
                ++count;
        }
    }
    return count;
}

} // namespace

class EmulatorTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void session()
    {
        EngineFolder engine;
        MugenEngine mugen;
        QString error;
        std::unique_ptr<Project> project(mugen.openProject(engine.def, &error));
        QVERIFY2(project, qPrintable(error));
        QCOMPARE(EmulatorSession::engineFolderOf(engine.def), QDir::cleanPath(engine.dir.path()));
        QCOMPARE(QFileInfo(EmulatorSession::defaultStage(engine.dir.path())).fileName(), QStringLiteral("stage0.def"));

        EmulatorSession session;
        EmulatorSession::Options options;
        options.engineDir = engine.dir.path();
        QVERIFY2(session.start(*project, options, &error), qPrintable(error));
        QCOMPARE(session.players().size(), std::size_t(2));
        QCOMPARE(session.players()[0].name, QStringLiteral("Tester"));
        QVERIFY(session.players()[0].sprites);
        QCOMPARE(session.match().roundState(), 0);
        QCOMPARE(session.stage().zoffset, 200.f);

        for (int i = 0; i < 200 && session.match().roundState() < 2; ++i)
            session.tick({}, {});
        QCOMPARE(session.match().roundState(), 2);
        QCOMPARE(session.players()[0].character->pos().x(), -50.0);
        QCOMPARE(session.players()[1].character->pos().x(), 50.0);
        QCOMPARE(session.match().roundTime(), 299); // mugen.cfg's 5 counts, from the first tick of the fight

        EmulatorRenderer renderer;
        const QImage frame = renderer.render(session);
        QCOMPARE(frame.size(), QSize(320, 240));
        QVERIFY(litPixels(frame) > 0);
        EmulatorSession idle;
        QCOMPARE(litPixels(renderer.render(idle)), 0);
    }

    // A tick and a drawn frame must leave room for 60 frames a second.
    void kungFuManFrameCost()
    {
        const QString folder = qEnvironmentVariable("OFS_CORPUS_MUGEN");
        if (folder.isEmpty() || !QFileInfo::exists(QDir(folder).filePath("chars/kfm/kfm.def")))
            QSKIP("OFS_CORPUS_MUGEN is not set to a M.U.G.E.N folder with chars/kfm");
        MugenEngine mugen;
        QString error;
        std::unique_ptr<Project> project(mugen.openProject(QDir(folder).filePath("chars/kfm/kfm.def"), &error));
        QVERIFY2(project, qPrintable(error));
        EmulatorSession session;
        EmulatorSession::Options options;
        options.engineDir = folder;
        QVERIFY2(session.start(*project, options, &error), qPrintable(error));
        EmulatorRenderer renderer;
        for (int i = 0; i < 200 && session.match().roundState() < 2; ++i)
            session.tick({}, {});
        qint64 tickNs = 0;
        qint64 renderNs = 0;
        QElapsedTimer clock;
        const int frames = 300;
        for (int i = 0; i < frames; ++i) {
            Runtime::InputState p1;
            p1.right = i % 60 < 30;
            p1.x = i % 20 == 0;
            clock.start();
            session.tick(p1, {});
            tickNs += clock.nsecsElapsed();
            clock.start();
            const QImage frame = renderer.render(session);
            renderNs += clock.nsecsElapsed();
            QVERIFY(!frame.isNull());
        }
        const double tickMs = tickNs / 1e6 / frames;
        const double renderMs = renderNs / 1e6 / frames;
        qInfo("a tick %.3f ms, a frame %.3f ms (%d frames)", tickMs, renderMs, frames);
        QVERIFY2(tickMs + renderMs < 1000.0 / 60, "a frame and its tick take longer than 1/60 s");
    }

    void restartIntoTraining()
    {
        const QString folder = qEnvironmentVariable("OFS_CORPUS_MUGEN");
        if (folder.isEmpty() || !QFileInfo::exists(QDir(folder).filePath("chars/kfm/kfm.def")))
            QSKIP("OFS_CORPUS_MUGEN is not set to a M.U.G.E.N folder with chars/kfm");
        MugenEngine mugen;
        QString error;
        std::unique_ptr<Project> project(mugen.openProject(QDir(folder).filePath("chars/kfm/kfm.def"), &error));
        QVERIFY2(project, qPrintable(error));
        EmulatorSession session;
        EmulatorRenderer renderer;
        const int seeds =
            qEnvironmentVariableIntValue("OFS_SEED_COUNT") > 0 ? qEnvironmentVariableIntValue("OFS_SEED_COUNT") : 6;
        for (int seed = 1; seed <= seeds; ++seed) {
            for (bool training : {false, true}) {
                EmulatorSession::Options options;
                options.engineDir = folder;
                options.training = training;
                options.seed = std::uint32_t(seed * 7919 + (training ? 1 : 0)) | 1u;
                try {
                    QVERIFY2(session.start(*project, options, &error), qPrintable(error));
                    for (int i = 0; i < 300; ++i) {
                        Runtime::InputState p1;
                        p1.right = i % 60 < 30;
                        p1.x = i % 20 == 0;
                        session.tick(p1, {});
                        if (i % 3 == 0)
                            renderer.render(session);
                    }
                } catch (const std::exception& e) {
                    QFAIL(qPrintable(QStringLiteral("seed %1, training %2: %3").arg(seed).arg(training).arg(e.what())));
                }
            }
        }
    }

    void unsavedEdits()
    {
        EngineFolder engine;
        MugenEngine mugen;
        std::unique_ptr<Project> project(mugen.openProject(engine.def, nullptr));
        QVERIFY(project);
        CodeFile* cns = project->codeFile(QDir::cleanPath(engine.dir.filePath("chars/test/test.cns")), "states");
        QVERIFY(cns);
        QTextCursor cursor(cns->document());
        cursor.select(QTextCursor::Document);
        cursor.insertText("[Data]\nlife = 600\n");
        EmulatorSession session;
        EmulatorSession::Options options;
        options.engineDir = engine.dir.path();
        QString error;
        QVERIFY2(session.start(*project, options, &error), qPrintable(error));
        QCOMPARE(session.players()[0].character->life(), 600);
    }

    void failures()
    {
        EngineFolder engine;
        MugenEngine mugen;
        std::unique_ptr<Project> project(mugen.openProject(engine.def, nullptr));
        EmulatorSession session;
        QString error;
        QVERIFY(!session.start(*project, {}, &error));
        QVERIFY(error.contains("engine directory"));
    }

    void widget()
    {
        EngineFolder engine;
        MugenEngine mugen;
        std::unique_ptr<Project> project(mugen.openProject(engine.def, nullptr));
        AppContext context;
        EmulatorWidget emulator(*project, context);
        QCOMPARE(emulator.engineDir(), QDir::cleanPath(engine.dir.path()));
        QVERIFY2(emulator.start(), qPrintable(emulator.lastError()));
        QVERIFY(emulator.isRunning());
        for (int i = 0; i < 10; ++i)
            emulator.step();
        QCOMPARE(emulator.frame().size(), QSize(320, 240));
        QCOMPARE(emulator.statusSections().value(2), QStringLiteral("Renderer: ") + Rendering::backendName());
        context.config().setValue(QStringLiteral("show_attack_box"), false);
        context.engineConfig(QStringLiteral("mugen"))
            .setValue(QStringLiteral("clsn_default_collision"), QStringLiteral("1,2,3"));
        EmulatorRenderer::Options look = emulator.renderOptions();
        QVERIFY(!look.attackBoxes);
        QVERIFY(look.collisionBoxes);
        QCOMPARE(look.collisionColor, QColor(1, 2, 3));
        emulator.applySettings();
        emulator.resize(700, 500);
        QCOMPARE(emulator.zoom(), 0);
        QWheelEvent up(QPointF(10, 10), QPointF(10, 10), QPoint(), QPoint(0, 120), Qt::NoButton, Qt::NoModifier,
                       Qt::NoScrollPhase, false);
        QCoreApplication::sendEvent(&emulator, &up);
        QCOMPARE(emulator.zoom(), 3);
        QWheelEvent down(QPointF(10, 10), QPointF(10, 10), QPoint(), QPoint(0, -120), Qt::NoButton, Qt::NoModifier,
                         Qt::NoScrollPhase, false);
        for (int i = 0; i < 3; ++i)
            QCoreApplication::sendEvent(&emulator, &down);
        QCOMPARE(emulator.zoom(), 0);
        emulator.stop();
        QVERIFY(!emulator.isRunning());
    }

    void sounds()
    {
        EngineFolder engine;
        writeSounds(engine.dir.filePath("chars/test/test.snd"), {{1, 0, 0.25}});
        writeSounds(engine.dir.filePath("data/common.snd"), {{5, 0, 0.5}});
        writeFile(engine.dir.filePath("data/fight.def"),
                  "[Files]\ncommon.snd = common.snd\n[Round]\nmatch.wins = 1\nstart.waittime = 3\nctrl.time = 3\n");
        writeFile(engine.dir.filePath("chars/test/test.cmd"),
                  "[Statedef -1]\n"
                  "[State -1]\ntype = PlaySnd\ntrigger1 = gametime = 2\nvalue = 1,0\nchannel = 2\n"
                  "[State -1]\ntype = PlaySnd\ntrigger1 = gametime = 3\nvalue = F5,0\n"
                  "[State -1]\ntype = PlaySnd\ntrigger1 = gametime = 4\nvalue = 9,9\n"
                  "[State -1]\ntype = StopSnd\ntrigger1 = gametime = 5\nchannel = 2\n");
        writeFile(engine.def, "[Info]\nname = \"Tester\"\n[Files]\nsprite = test.sff\nanim = test.air\ncns = test.cns\n"
                              "st = test.cns\ncmd = test.cmd\nstcommon = common1.cns\nsound = test.snd\n");
        MugenEngine mugen;
        std::unique_ptr<Project> project(mugen.openProject(engine.def, nullptr));
        QVERIFY(project);
        EmulatorSession session;
        EmulatorSession::Options options;
        options.engineDir = engine.dir.path();
        QString error;
        QVERIFY2(session.start(*project, options, &error), qPrintable(error));
        QVERIFY(session.players()[0].sounds);
        QVERIFY(session.commonSounds());
        QVERIFY(session.sound(session.players()[0].character->id(), false, 1, 0));
        QVERIFY(session.sound(0, true, 5, 0));
        QVERIFY(!session.sound(0, true, 1, 0));

        qputenv("OFS_AUDIO_BACKEND", "null");
        AudioMixer mixer(AudioEngine::Backend::Null);
        QVERIFY(mixer.isAvailable());
        int most = 0;
        for (int i = 0; i < 5; ++i) {
            session.tick({}, {});
            session.playSounds(mixer);
            most = std::max(most, mixer.voices());
        }
        QCOMPARE(most, 4);
        const std::vector<float> out = mixer.mix(1);
        QVERIFY(out[0] > 1.0f && out[0] < 1.4f);
        QVERIFY(std::abs(out[0] - out[1]) < 0.01f);
        session.tick({}, {});
        session.playSounds(mixer);
        QCOMPARE(mixer.voices(), 2);

        const int p1 = session.players()[0].character->id();
        QVERIFY(session.stereoPan(p1, 0, false) < 0);
        QVERIFY(session.stereoPan(p1, 100, false) > session.stereoPan(p1, 0, false));
        QCOMPARE(session.stereoPan(p1, 120, true), 1.f);

        AppContext context;
        EmulatorWidget emulator(*project, context);
        QVERIFY2(emulator.start(), qPrintable(emulator.lastError()));
        QVERIFY(emulator.mixer());
        for (int i = 0; i < 4; ++i)
            emulator.step();
        QVERIFY(emulator.mixer()->voices() > 0);
        emulator.stop();
        QCOMPARE(emulator.mixer()->voices(), 0);
    }

    void fightScreen()
    {
        EngineFolder engine;
        Formats::Test::FixtureFactory fixtures;
        QVERIFY(fixtures.sff(Formats::SffVersion::V1, 4, 16)
                    .save(engine.dir.filePath("data/fight.sff").toStdWString())
                    .ok());
        writeSounds(engine.dir.filePath("data/fight.snd"), {{0, 1, 0.25}, {1, 0, 0.5}});
        Formats::Palette palette(256);
        palette[1] = Formats::Rgba{255, 255, 255, 255};
        Formats::FntDocument fnt;
        fnt.pcx = *Formats::encodePcx(Formats::Image::indexed(30, 5, std::vector<std::uint8_t>(150, 1), palette));
        fnt.text = "[Def]\nSize = 3,5\nSpacing = 1,0\nColors = 1\nOffset = 0,0\nType = Variable\n[Map]\n";
        for (int digit = 0; digit < 10; ++digit)
            fnt.text += std::to_string(digit) + " " + std::to_string(digit * 3) + " 3\n";
        QVERIFY(fnt.save(engine.dir.filePath("data/digits.fnt").toStdWString()).ok());
        writeFile(engine.dir.filePath("data/fight.def"),
                  "[Files]\nsff = fight.sff\nsnd = fight.snd\nfont1 = digits.fnt\n"
                  "[Lifebar]\np1.pos = 140,12\np1.front.spr = 0,0\np1.range.x = 0,-127\n"
                  "p2.pos = 178,12\np2.front.spr = 0,1\np2.range.x = 0,127\n"
                  "[Time]\npos = 160,23\ncounter.font = 1,0,0\n"
                  "[Round]\nmatch.wins = 1\nstart.waittime = 3\nctrl.time = 3\n"
                  "round.default.offset = 160,100\nround.default.font = 1,0,0\nround.default.text = Round %i\n"
                  "round.default.displaytime = 20\nround1.snd = 0,1\n"
                  "fight.offset = 160,110\nfight.anim = 80\nfight.snd = 1,0\n"
                  "KO.offset = 160,70\nKO.font = 1,0\nKO.text = 0\nKO.displaytime = 30\n"
                  "[Begin Action 80]\n0,2, 0,0, 5\n0,3, 0,0, 5\n");
        writeFile(engine.dir.filePath("chars/test/test.cmd"),
                  "[Statedef -1]\n"
                  "[State -1]\ntype = VarSet\ntrigger1 = roundstate = 2 && pos x > 0 && life = 1000\nv = 0\n"
                  "value = gametime\n"
                  "[State -1]\ntype = LifeAdd\ntrigger1 = roundstate = 2 && pos x > 0 && life = 1000\nvalue = -500\n"
                  "[State -1]\ntype = LifeAdd\ntrigger1 = pos x > 0 && life = 500 && gametime >= var(0) + 20\n"
                  "value = -500\n");
        MugenEngine mugen;
        std::unique_ptr<Project> project(mugen.openProject(engine.def, nullptr));
        QVERIFY(project);
        EmulatorSession session;
        EmulatorSession::Options options;
        options.engineDir = engine.dir.path();
        QString error;
        QVERIFY2(session.start(*project, options, &error), qPrintable(error));
        const FightScreen& screen = session.fightScreen();
        QVERIFY(screen.isLoaded());
        QCOMPARE(screen.callTime(), 60);
        QVERIFY(screen.font(1));
        QCOMPARE(screen.font(1)->textWidth("12"), 7);

        auto findText = [](const QList<FightScreen::Item>& items, const QString& text) {
            return std::find_if(items.cbegin(), items.cend(),
                                [&](const FightScreen::Item& i) { return i.font && i.text == text; }) != items.cend();
        };
        auto findSprite = [](const QList<FightScreen::Item>& items, int group, int number) {
            return std::find_if(items.cbegin(), items.cend(), [&](const FightScreen::Item& i) {
                return i.sprites && i.group == group && i.number == number;
            });
        };
        int roundShown = -1;
        int fightShown = -1;
        int fighting = -1;
        QList<int> announced;
        for (int tick = 0; tick < 200 && fighting < 0; ++tick) {
            session.tick({}, {});
            const QList<FightScreen::Item> items = screen.items(session.match(), session.sides());
            if (roundShown < 0 && findText(items, "Round 1"))
                roundShown = tick;
            if (fightShown < 0 && findSprite(items, 0, 2) != items.cend())
                fightShown = tick;
            if (session.match().roundState() == 2)
                fighting = tick;
            for (const FightScreen::Sound& s : screen.sounds())
                announced << s.group;
            // Whole counts left: 4 once the fight starts.
            QVERIFY(findText(items, fighting < 0 ? "5" : "4"));
        }
        QVERIFY(roundShown > 0);
        QCOMPARE(fightShown, roundShown + 60);
        QCOMPARE(fighting, fightShown + 2); // ctrl.time (3) from the call's first tick
        QCOMPARE(announced, QList<int>({0, 1}));

        for (int i = 0; i < 10; ++i)
            session.tick({}, {});
        QCOMPARE(session.players()[1].character->life(), 500);
        QList<FightScreen::Item> items = screen.items(session.match(), session.sides());
        auto front = findSprite(items, 0, 1);
        QVERIFY(front != items.cend() && front->clip);
        QCOMPARE(front->clip->left(), 178.0);
        QCOMPARE(front->clip->width(), 64.0);
        QVERIFY(findSprite(items, 0, 0)->clip->width() == 128.0);

        bool ko = false;
        for (int i = 0; i < 100 && !ko; ++i) {
            session.tick({}, {});
            ko = findText(screen.items(session.match(), session.sides()), "0");
        }
        QVERIFY(ko);
        QCOMPARE(session.match().roundEnd(), Runtime::RoundEnd::KO);
        QCOMPARE(session.match().overClock(), 10);
        QCOMPARE(screen.gameSpeed(session.match()), 0.25f);
        while (session.match().overClock() < 59)
            session.tick({}, {});
        QVERIFY(screen.gameSpeed(session.match()) > 0.9f);
        session.tick({}, {});
        QCOMPARE(screen.gameSpeed(session.match()), 1.f);

        EmulatorRenderer renderer;
        const QImage frame = renderer.render(session);
        QVERIFY(litPixels(frame.copy(0, 0, 320, 40)) > 0);
    }

    void explodModes()
    {
        EngineFolder engine;
        const QString dir = engine.dir.path();
        QVERIFY(!EmulatorSession::isMugen11Engine(dir));
        QCOMPARE(EmulatorSession::explodRules(0, dir), Runtime::ExplodRules::Mugen10);
        writeFile(engine.dir.filePath("data/mugen.cfg"), "[Options]\ntime = 5\n[Video]\nStageFit = 1\n");
        QVERIFY(EmulatorSession::isMugen11Engine(dir));
        QCOMPARE(EmulatorSession::explodRules(0, dir), Runtime::ExplodRules::PlayerVersion);
        QCOMPARE(EmulatorSession::explodRules(1, dir), Runtime::ExplodRules::Mugen10);
        QCOMPARE(EmulatorSession::explodRules(2, dir), Runtime::ExplodRules::Mugen11);
        QCOMPARE(EmulatorSession::explodRules(3, dir), Runtime::ExplodRules::Mugen11);
        QCOMPARE(EmulatorSession::explodRules(4, dir), Runtime::ExplodRules::PlayerVersion);
        MugenEngine mugen;
        std::unique_ptr<Project> project(mugen.openProject(engine.def, nullptr));
        EmulatorSession session;
        EmulatorSession::Options options;
        options.engineDir = dir;
        options.explodMode = 2;
        QString error;
        QVERIFY2(session.start(*project, options, &error), qPrintable(error));
        QCOMPARE(session.match().explodRules(), Runtime::ExplodRules::Mugen11);
    }

    void training()
    {
        EngineFolder engine;
        MugenEngine mugen;
        std::unique_ptr<Project> project(mugen.openProject(engine.def, nullptr));
        QVERIFY(project);
        EmulatorSession session;
        EmulatorSession::Options options;
        options.engineDir = engine.dir.path();
        options.training = true;
        options.trainingOptions.control = TrainingOptions::Control::AI;
        options.trainingOptions.aiLevel = 6;
        QString error;
        QVERIFY2(session.start(*project, options, &error), qPrintable(error));
        QVERIFY(session.match().noKo());
        QCOMPARE(session.fightScreen().callTime(), 0);
        QCOMPARE(session.players()[1].character->aiLevel(), 6);
        QCOMPARE(session.players()[0].character->aiLevel(), 0);
        for (int i = 0; i < 40 && session.match().roundState() < 2; ++i)
            session.tick({}, {});
        QCOMPARE(session.match().roundState(), 2);
        QCOMPARE(session.match().roundTime(), -1);
        TrainingOptions cooperative;
        session.setTrainingOptions(cooperative);
        QCOMPARE(session.players()[1].character->aiLevel(), 0);
        QCOMPARE(session.training().options().control, TrainingOptions::Control::Cooperative);
    }

    void renderEffects()
    {
        // Var(10) picks the effect the -1 states apply, over a grey EnvColor screen.
        EngineFolder engine;
        Formats::Test::FixtureFactory fixtures;
        SpriteDocument sprites = SpriteDocument::fromSff(fixtures.sff(Formats::SffVersion::V2, 1, 8));
        Formats::Palette own(256);
        own[1] = Formats::Rgba{100, 50, 200, 255};
        Formats::Palette other(256);
        other[1] = Formats::Rgba{10, 200, 10, 255};
        sprites.palettes() = {SpriteDocument::makePalette(1, 1, own), SpriteDocument::makePalette(1, 2, other)};
        SpriteEdits::setImage(sprites, 0, Formats::Image::indexed(10, 10, std::vector<std::uint8_t>(100, 1), own));
        sprites.sprites()[0].palette = sprites.palettes()[0];
        SpriteEdits::setIdentifiers(sprites, 0, 0, 0);
        SpriteEdits::setAxis(sprites, 0, 5, 10);
        QVERIFY(sprites.toSff().save(engine.dir.filePath("chars/test/test.sff").toStdWString()).ok());
        writeFile(engine.dir.filePath("chars/test/test.air"),
                  "[Begin Action 0]\nClsn2: 1\n Clsn2[0] = -20,-30,20,0\n0,0, 0,0, -1\n");
        writeFile(engine.dir.filePath("chars/test/test.cmd"),
                  "[Statedef -1]\n"
                  "[State -1]\ntype = EnvColor\ntrigger1 = 1\nvalue = 200,200,200\ntime = 1\nunder = 1\n"
                  "[State -1]\ntype = PalFX\ntrigger1 = var(10) = 1\nadd = 50,0,0\nmul = 256,128,256\ntime = 1\n"
                  "[State -1]\ntype = Trans\ntrigger1 = var(10) = 2\ntrans = sub\n"
                  "[State -1]\ntype = Trans\ntrigger1 = var(10) = 3\ntrans = add1\n"
                  "[State -1]\ntype = RemapPal\ntrigger1 = var(10) = 4\nsource = 1,1\ndest = 1,2\n"
                  "[State -1]\ntype = RemapPal\ntrigger1 = var(10) != 4\nsource = 1,1\ndest = 1,1\n"
                  "[State -1]\ntype = AfterImage\ntrigger1 = var(10) = 5 && var(11) = 0\ntime = -1\n"
                  "timegap = 1\nframegap = 2\nlength = 10\n"
                  "[State -1]\ntype = VarSet\ntrigger1 = var(10) = 5\nv = 11\nvalue = 1\n"
                  "[State -1]\ntype = PosAdd\ntrigger1 = var(10) = 5 && roundstate = 2\nx = 3\n");
        MugenEngine mugen;
        std::unique_ptr<Project> project(mugen.openProject(engine.def, nullptr));
        QVERIFY(project);
        EmulatorSession session;
        EmulatorSession::Options options;
        options.engineDir = engine.dir.path();
        options.training = true;
        QString error;
        QVERIFY2(session.start(*project, options, &error), qPrintable(error));
        for (int i = 0; i < 40 && session.match().roundState() < 2; ++i)
            session.tick({}, {});
        Runtime::Character& p1 = *session.players()[0].character;
        EmulatorRenderer renderer;
        auto sample = [&](int effect) {
            p1.setVar(10, effect);
            session.tick({}, {});
            session.tick({}, {});
            const QImage frame = renderer.render(session);
            const QPointF at(p1.pos().x() - session.match().cameraX() + 160, p1.pos().y() + 200 - 5);
            return frame.pixel(at.toPoint());
        };
        auto rgb = [](QRgb c) {
            return QList<int>{qRed(c), qGreen(c), qBlue(c)};
        };
        QCOMPARE(rgb(sample(0)), QList<int>({100, 50, 200}));
        // PalFX: (colour + add) * mul / 256.
        QCOMPARE(rgb(sample(1)), QList<int>({150, 25, 200}));
        // Subtraction from the grey, and A1: added to the grey darkened by half.
        QCOMPARE(rgb(sample(2)), QList<int>({100, 150, 0}));
        QCOMPARE(rgb(sample(3)), QList<int>({200, 150, 255}));
        QCOMPARE(rgb(sample(4)), QList<int>({10, 200, 10}));
        QCOMPARE(rgb(sample(0)), QList<int>({100, 50, 200}));
        for (int i = 0; i < 6; ++i)
            sample(5);
        QVERIFY(p1.afterImages().size() >= 6);
        const QImage frame = renderer.render(session);
        const int x = int(p1.pos().x() - session.match().cameraX() + 160);
        const QRgb trail = frame.pixel(x - 12, 195);
        QVERIFY(trail != qRgb(200, 200, 200));
        QVERIFY(qBlue(trail) < 200 && qRed(trail) < 100);
        QCOMPARE(rgb(frame.pixel(x, 195)), QList<int>({100, 50, 200}));

        EmulatorRenderer::Options boxes;
        boxes.collisionBoxes = true;
        boxes.collisionColor = QColor(0, 255, 255);
        const QImage withBoxes = renderer.render(session, boxes);
        QCOMPARE(rgb(withBoxes.pixel(x - 20, 200 - 30)), QList<int>({0, 255, 255}));
        QVERIFY(withBoxes.pixel(x, 200 - 30) == qRgb(0, 255, 255));
        QVERIFY(renderer.render(session).pixel(x - 20, 200 - 30) != qRgb(0, 255, 255));
    }

    void actPalettes()
    {
        EngineFolder engine;
        Formats::Test::FixtureFactory fixtures;
        Formats::SffDocument sff = fixtures.sff(Formats::SffVersion::V1, 2, 24);
        sff.sprites[1].samePalette = true;
        sff.sprites[1].paletteIndex = sff.sprites[0].paletteIndex;
        QVERIFY(sff.save(engine.dir.filePath("chars/test/test.sff").toStdWString()).ok());
        Formats::Palette act(256);
        act[1] = Formats::Rgba{1, 2, 3, 255};
        const Formats::Bytes bytes = Formats::writeAct(act, Formats::ActOrder::Reversed);
        writeFile(engine.dir.filePath("chars/test/test.act"),
                  QByteArray(reinterpret_cast<const char*>(bytes.data()), qsizetype(bytes.size())));
        writeFile(engine.def, "[Info]\nname = \"Tester\"\n[Files]\nsprite = test.sff\nanim = test.air\ncns = test.cns\n"
                              "st = test.cns\ncmd = test.cmd\nstcommon = common1.cns\npal1 = test.act\n");
        MugenEngine mugen;
        std::unique_ptr<Project> project(mugen.openProject(engine.def, nullptr));
        EmulatorSession session;
        EmulatorSession::Options options;
        options.engineDir = engine.dir.path();
        QString error;
        QVERIFY2(session.start(*project, options, &error), qPrintable(error));
        const EmulatorSession::Player& player = session.players()[0];
        QVERIFY(player.acts[0]);
        const auto& entries = player.sprites->sprites();
        const Formats::Palette* palette = session.paletteOf(*player.character, entries[1].group(), entries[1].number());
        QVERIFY(palette);
        QCOMPARE(int((*palette)[1].b), 3);
    }

    void debugger()
    {
        EngineFolder engine;
        writeFile(engine.dir.filePath("chars/test/test.cmd"), kDebugCmd);
        const QString cmd = QDir::cleanPath(engine.dir.filePath("chars/test/test.cmd"));
        const QString common = QDir::cleanPath(engine.dir.filePath("data/common1.cns"));
        MugenEngine mugen;
        std::unique_ptr<Project> project(mugen.openProject(engine.def, nullptr));
        AppContext context;
        Debug::BreakpointList breakpoints;
        EmulatorWidget emulator(*project, context);
        emulator.setBreakpoints(&breakpoints);
        QVERIFY2(emulator.start(), qPrintable(emulator.lastError()));
        QCOMPARE(emulator.debugState(), Debug::SessionState::Running);
        for (int i = 0; i < 30 && emulator.session().match().roundState() < 2; ++i)
            emulator.step();
        const int p1 = emulator.session().players()[0].character->id();
        const int p2 = emulator.session().players()[1].character->id();
        QList<std::pair<QString, int>> lines;
        connect(&emulator, &EngineWorkspace::debugLineChanged, this,
                [&lines](const QString& file, int line) { lines << std::pair(file, line); });

        breakpoints.toggle(cmd, 4);
        auto at = [&emulator]() {
            return emulator.debugger().location().value_or(EmulatorDebugger::Location());
        };
        int answered = 0;
        whileStopped(
            emulator,
            {[&] {
                 QCOMPARE(at().line, 4);
                 QCOMPARE(at().file, cmd);
                 QCOMPARE(at().characterId, p1);
                 QCOMPARE(at().stateNo, -1);
                 QCOMPARE(emulator.debugStack().size(), 2);
                 QCOMPARE(emulator.debugStack().at(0).line, 4);
                 QCOMPARE(emulator.debugStack().at(1).line, 1);
                 const auto var0 = emulator.debugVariable(QStringLiteral("%1.var(0)").arg(p1));
                 QVERIFY(var0);
                 QCOMPARE(var0->type, QStringLiteral("int"));
                 QVERIFY(emulator.debugVariables().size() >= 3);
                 emulator.perform("debug_step_into");
             },
             [&] {
                 QCOMPARE(at().line, 7);
                 emulator.perform("debug_step_over");
             },
             [&] {
                 QCOMPARE(at().line, 9);
                 emulator.perform("debug_step_out");
             },
             [&] {
                 QCOMPARE(at().stateNo, 0);
                 QCOMPARE(at().file, common);
                 emulator.perform("debug_continue");
             },
             [&] {
                 QCOMPARE(at().characterId, p2);
                 QCOMPARE(at().line, 4);
                 breakpoints.removeAll();
                 emulator.perform("debug_continue");
             }},
            [&] { emulator.step(); }, &answered);
        QCOMPARE(answered, 5);
        QCOMPARE(emulator.debugState(), Debug::SessionState::Running);
        QVERIFY(!lines.isEmpty());
        QCOMPARE(lines.first(), std::pair(cmd, 4));
        QCOMPARE(lines.last(), std::pair(QString(), 0));

        Debug::Breakpoint conditional;
        conditional.file = cmd;
        conditional.line = 10;
        conditional.condition = QStringLiteral("var(0) % 2 = 0");
        conditional.ignoreCount = 1;
        conditional.oneShot = true;
        const int number = breakpoints.add(conditional);
        QList<int> stoppedAt;
        whileStopped(
            emulator, {[&] {
                stoppedAt << emulator.session().match().playerById(at().characterId)->var(0);
                emulator.perform("debug_continue");
            }},
            [&] {
                for (int i = 0; i < 6; ++i)
                    emulator.step();
            },
            &answered);
        QCOMPARE(answered, 1);
        QCOMPARE(stoppedAt.size(), 1);
        QCOMPARE(stoppedAt.first() % 2, 0);
        QVERIFY(!breakpoints.find(number)->enabled);
        breakpoints.removeAll();

        Debug::Breakpoint data;
        data.type = Debug::BreakpointType::Memory;
        data.address = QStringLiteral("%1.var(1)").arg(p2);
        breakpoints.add(data);
        int before = 0;
        whileStopped(
            emulator, {[&] {
                QVERIFY(emulator.session().match().playerById(p2)->var(1) != before);
                breakpoints.removeAll();
                emulator.perform("debug_continue");
            }},
            [&] {
                emulator.step();
                before = emulator.session().match().playerById(p2)->var(1);
                emulator.step();
            },
            &answered);
        QCOMPARE(answered, 1);

        emulator.perform("debug_interrupt");
        int time = emulator.session().match().gameTime();
        emulator.step();
        QCOMPARE(emulator.debugState(), Debug::SessionState::Interrupted);
        QVERIFY(!emulator.debugger().location());
        QCOMPARE(emulator.session().match().gameTime(), time);
        emulator.perform("debug_step_frame");
        emulator.step();
        QCOMPARE(emulator.session().match().gameTime(), time + 1);
        emulator.step();
        QCOMPARE(emulator.debugState(), Debug::SessionState::Interrupted);
        QCOMPARE(emulator.session().match().gameTime(), time + 1);
        whileStopped(
            emulator, {[&] {
                QCOMPARE(at().line, 2);
                emulator.perform("debug_continue");
            }},
            [&] {
                emulator.perform("debug_step_into");
                emulator.step();
            },
            &answered);
        QCOMPARE(answered, 1);
        time = emulator.session().match().gameTime();
        emulator.step();
        QCOMPARE(emulator.session().match().gameTime(), time + 1);

        breakpoints.toggle(cmd, 4);
        whileStopped(
            emulator, {[&] {
                emulator.perform("debug_stop");
            }},
            [&] { emulator.step(); }, &answered);
        QCOMPARE(answered, 1);
        QVERIFY(!emulator.isRunning());
        QCOMPARE(emulator.debugState(), Debug::SessionState::None);

        auto* doomed = new EmulatorWidget(*project, context);
        doomed->setBreakpoints(&breakpoints);
        QVERIFY(doomed->start());
        QPointer<EmulatorWidget> watch(doomed);
        whileStopped(
            *doomed, {[&] {
                delete doomed;
            }},
            [&] { doomed->step(); }, &answered);
        QCOMPARE(answered, 1);
        QVERIFY(!watch);
    }

    void otherProjects()
    {
        EngineFolder engine;
        writeFile(engine.dir.filePath("data/fight.def"),
                  "[Lifebar]\np1.pos = 140,12\n[Round]\nmatch.wins = 1\nstart.waittime = 3\nctrl.time = 3\n");
        QCOMPARE(QFileInfo(EmulatorSession::defaultCharacter(engine.dir.path())).fileName(),
                 QStringLiteral("test.def"));
        QVERIFY(EmulatorSession::canPlay("stage"));
        QVERIFY(!EmulatorSession::canPlay("storyboard"));
        MugenEngine mugen;
        QString error;

        const QString stageDef = QDir::cleanPath(engine.dir.filePath("stages/stage0.def"));
        std::unique_ptr<Project> stage(mugen.openProject(stageDef, &error));
        QVERIFY2(stage, qPrintable(error));
        QCOMPARE(stage->typeId(), QStringLiteral("stage"));
        CodeFile* stageText = stage->codeFile(stageDef, "definitions");
        QVERIFY(stageText);
        QTextCursor cursor(stageText->document());
        cursor.select(QTextCursor::Document);
        cursor.insertText("[StageInfo]\nzoffset = 180\n[Camera]\nboundleft = -100\nboundright = 100\n"
                          "[PlayerInfo]\np1startx = -30\np2startx = 30\n[BGdef]\nspr = stage0.sff\n");
        EmulatorSession session;
        EmulatorSession::Options options;
        options.engineDir = engine.dir.path();
        QVERIFY2(session.start(*stage, options, &error), qPrintable(error));
        QCOMPARE(session.players()[0].name, QStringLiteral("Tester"));
        QCOMPARE(session.stage().zoffset, 180.f);
        QCOMPARE(session.stage().path, stageDef);

        const QString fightDef = QDir::cleanPath(engine.dir.filePath("data/fight.def"));
        std::unique_ptr<Project> fight(mugen.openProject(fightDef, &error));
        QVERIFY2(fight, qPrintable(error));
        QCOMPARE(fight->typeId(), QStringLiteral("fight"));
        CodeFile* fightText = fight->codeFile(fightDef, "definitions");
        QVERIFY(fightText);
        QTextCursor fightCursor(fightText->document());
        fightCursor.select(QTextCursor::Document);
        fightCursor.insertText("[Lifebar]\np1.pos = 140,12\n[Round]\nmatch.wins = 1\nstart.waittime = 3\n"
                               "ctrl.time = 3\nround.time = 10\n");
        EmulatorSession fighting;
        QVERIFY2(fighting.start(*fight, options, &error), qPrintable(error));
        QCOMPARE(fighting.fightScreen().callTime(), 70);

        AppContext context;
        EmulatorWidget emulator(*stage, context);
        QVERIFY2(emulator.start(), qPrintable(emulator.lastError()));
        emulator.stop();
    }

    void storyboard()
    {
        EngineFolder engine;
        QDir().mkpath(engine.dir.filePath("story"));
        SpriteDocument sprites;
        Formats::Palette colors(256);
        colors[1] = Formats::Rgba{30, 160, 90, 255};
        colors[2] = Formats::Rgba{250, 250, 250, 255};
        sprites.palettes() << SpriteDocument::makePalette(1, 1, colors);
        SpriteEntry square;
        square.header.axisX = 5;
        square.header.axisY = 5;
        square.pixels =
            SpriteDocument::makePixels(Formats::Image::indexed(10, 10, std::vector<std::uint8_t>(100, 1), colors));
        square.palette = sprites.palettes().first();
        sprites.sprites() << square;
        SpriteEntry wide = square;
        wide.header.group = 5;
        wide.pixels =
            SpriteDocument::makePixels(Formats::Image::indexed(40, 4, std::vector<std::uint8_t>(160, 2), colors));
        sprites.sprites() << wide;
        QVERIFY(sprites.toSff().save(engine.dir.filePath("story/intro.sff").toStdWString()).ok());
        writeSounds(engine.dir.filePath("story/intro.snd"), {{1, 0, 0.5}});
        Formats::Palette white(256);
        white[1] = Formats::Rgba{255, 255, 255, 255};
        Formats::FntDocument fnt;
        fnt.pcx = *Formats::encodePcx(Formats::Image::indexed(30, 5, std::vector<std::uint8_t>(150, 1), white));
        fnt.text = "[Def]\nSize = 3,5\nSpacing = 1,0\nColors = 1\nOffset = 0,0\nType = Variable\n[Map]\n";
        for (int digit = 0; digit < 10; ++digit)
            fnt.text += std::to_string(digit) + " " + std::to_string(digit * 3) + " 3\n";
        QVERIFY(fnt.save(engine.dir.filePath("story/digits.fnt").toStdWString()).ok());
        const QString def = engine.dir.filePath("story/intro.def");
        writeFile(def, "[Info]\nlocalcoord = 320,240\n"
                       "[SceneDef]\nspr = intro.sff\nsnd = intro.snd\nfont1 = digits.fnt\n"
                       "[Scene 0]\nclearcolor = 0,0,0\nlayerall.pos = 160,120\nlayer0.anim = 0\n"
                       "layer1.text = \"12\\n34\"\nlayer1.font = 1,0,1\nlayer1.offset = 20,0\nlayer1.textdelay = 2\n"
                       "sound0.value = 1,0\nsound0.starttime = 3\nfadein.time = 10\nend.time = 20\n"
                       "[Scene 1]\nlayer0.anim = 0\nlayer0.starttime = 5\nend.time = 10\nbg.name = sceneBg\n"
                       "[sceneBgDef]\n[sceneBg wide]\ntype = normal\nspriteno = 5,0\nstart = 0,200\n"
                       "[Begin Action 0]\n0,0, 0,0, -1\n");
        MugenEngine mugen;
        QString error;
        std::unique_ptr<Project> project(mugen.openProject(def, &error));
        QVERIFY2(project, qPrintable(error));
        QCOMPARE(project->typeId(), QStringLiteral("storyboard"));
        AppContext context;
        EmulatorWidget emulator(*project, context);
        QVERIFY2(emulator.start(), qPrintable(emulator.lastError()));
        QVERIFY(emulator.playsStoryboard());
        const StoryboardPlayer& story = emulator.storyboard();
        QCOMPARE(story.scenes().size(), 2);
        QCOMPARE(story.canvasSize(), QSize(320, 240));
        QImage frame = emulator.frame();
        QCOMPARE(qGreen(frame.pixel(160, 120)), 0);
        for (int i = 0; i < 3; ++i)
            emulator.step();
        QVERIFY(emulator.mixer()->voices() > 0);
        frame = emulator.frame();
        QVERIFY(qGreen(frame.pixel(160, 120)) > 0 && qGreen(frame.pixel(160, 120)) < 160);
        for (int i = 0; i < 9; ++i)
            emulator.step();
        frame = emulator.frame();
        QCOMPARE(qGreen(frame.pixel(160, 120)), 160);
        QVERIFY(qRed(frame.pixel(181, 118)) == 255); // "1" at x 180..182, above the base line
        QVERIFY(qRed(frame.pixel(181, 118 + 6)) == 255);
        for (int i = 0; i < 8; ++i)
            emulator.step();
        QCOMPARE(story.sceneIndex(), 1);
        frame = emulator.frame();
        QCOMPARE(qRed(frame.pixel(160, 198)), 250);
        QCOMPARE(qGreen(frame.pixel(160, 120)), 0);
        for (int i = 0; i < 6; ++i)
            emulator.step();
        QCOMPARE(qGreen(emulator.frame().pixel(160, 120)), 160);
        for (int i = 0; i < 5 && emulator.isRunning(); ++i)
            emulator.step();
        QVERIFY(story.isFinished());
        QVERIFY(!emulator.isRunning());
        QVERIFY(!emulator.frame().isNull());
    }

    void motifScreens()
    {
        EngineFolder engine;
        Formats::Test::FixtureFactory fixtures;
        QVERIFY(
            fixtures.sff(Formats::SffVersion::V1, 1).save(engine.dir.filePath("data/system.sff").toStdWString()).ok());
        writeSounds(engine.dir.filePath("data/system.snd"), {{100, 0, 0.5}, {100, 1, 0.5}});
        const QString system = engine.dir.filePath("data/system.def");
        writeFile(system, "[Info]\nlocalcoord = 320,240\n[Files]\nspr = system.sff\nsnd = system.snd\n"
                          "[Title Info]\nmenu.pos = 160,150\nmenu.item.spacing = 0,13\nmenu.itemname.teamcoop =\n"
                          "menu.window.visibleitems = 3\nmenu.window.margins.y = 12,8\n"
                          "cursor.move.snd = 100,0\ncursor.done.snd = 100,1\n[TitleBGdef]\n");
        MugenEngine mugen;
        QString error;
        std::unique_ptr<Project> motif(mugen.openProject(system, &error));
        QVERIFY2(motif, qPrintable(error));
        QCOMPARE(motif->typeId(), QStringLiteral("motif"));
        MotifScreen menu;
        QVERIFY2(menu.start(*motif, system, engine.dir.path(), MotifScreen::Screen::Menu, &error), qPrintable(error));
        QCOMPARE(menu.menuItems().size(), 10);
        QCOMPARE(menu.menuItems().first(), QStringLiteral("ARCADE"));
        QVERIFY(!menu.menuItems().contains("TEAM CO-OP"));
        qputenv("OFS_AUDIO_BACKEND", "null");
        AudioMixer mixer(AudioEngine::Backend::Null);
        Runtime::InputState up;
        up.up = true;
        menu.tick(up);
        menu.playSounds(mixer);
        QCOMPARE(menu.menuIndex(), 9);
        QCOMPARE(mixer.voices(), 1);
        menu.tick(up);
        QCOMPARE(menu.menuIndex(), 9);
        menu.tick({});
        menu.tick(up);
        QCOMPARE(menu.menuIndex(), 8);
        QCOMPARE(menu.canvasSize(), QSize(320, 240));

        AppContext context;
        EmulatorWidget emulator(*motif, context);
        QWidget panel;
        auto* screens = new QComboBox(&panel);
        screens->setObjectName("emul_screen");
        emulator.bindPanel(&panel);
        QVERIFY(screens->isEnabled());
        QCOMPARE(screens->count(), 6);
        QVERIFY2(emulator.start(), qPrintable(emulator.lastError()));
        QVERIFY(emulator.showsMotif());
        QCOMPARE(emulator.motifScreen().screen(), MotifScreen::Screen::Menu);
        screens->setCurrentIndex(1);
        QCOMPARE(emulator.motifScreen().screen(), MotifScreen::Screen::Versus);
        emulator.step();
        QVERIFY(!emulator.frame().isNull());
        emulator.stop();
        std::unique_ptr<Project> character(mugen.openProject(engine.def, nullptr));
        EmulatorWidget fighter(*character, context);
        QWidget otherPanel;
        auto* off = new QComboBox(&otherPanel);
        off->setObjectName("emul_screen");
        fighter.bindPanel(&otherPanel);
        QVERIFY(!off->isEnabled());
    }

    void kungFuMan()
    {
        // Set OFS_EMULATOR_FRAME to save the frame for a look.
        const QString engineDir = qEnvironmentVariable("OFS_CORPUS_MUGEN");
        const QString def = QDir(engineDir).filePath("chars/kfm/kfm.def");
        if (engineDir.isEmpty() || !QFileInfo::exists(def))
            QSKIP("OFS_CORPUS_MUGEN is not set to a M.U.G.E.N folder with chars/kfm");
        MugenEngine mugen;
        QString error;
        std::unique_ptr<Project> project(mugen.openProject(def, &error));
        QVERIFY2(project, qPrintable(error));
        EmulatorSession session;
        EmulatorSession::Options options;
        options.engineDir = EmulatorSession::engineFolderOf(def);
        QVERIFY2(session.start(*project, options, &error), qPrintable(error));
        QVERIFY(session.fightFx());
        QVERIFY(session.stage().sprites);
        const SoundDocument* sounds = session.players()[0].sounds;
        QVERIFY(sounds && sounds->count() > 0);
        const SoundEntry& first = sounds->sounds().first();
        QVERIFY(session.sound(session.players()[0].character->id(), false, first.group, first.number));
        QVERIFY(session.sound(0, true, 5, 0));
        EmulatorRenderer renderer;
        QImage frame;
        const QString out = qEnvironmentVariable("OFS_EMULATOR_FRAME");
        QVERIFY(session.fightScreen().isLoaded());
        bool roundCalled = false;
        bool fightCalled = false;
        for (int i = 0; i < 400 && session.match().roundState() < 2; ++i) {
            session.tick({}, {});
            const QList<FightScreen::Item> items = session.fightScreen().items(session.match(), session.sides());
            for (const FightScreen::Item& item : items) {
                const bool round = item.font && item.text == QLatin1String("Round 1");
                const bool fight = !item.font && item.group == 80;
                if ((round && !roundCalled) || (fight && !fightCalled)) {
                    if (!out.isEmpty())
                        renderer.render(session).scaled(960, 720).save(
                            QString(out).replace(".png", round ? "_round.png" : "_fight.png"));
                }
                roundCalled = roundCalled || round;
                fightCalled = fightCalled || fight;
            }
        }
        QVERIFY(roundCalled);
        QVERIFY(fightCalled);
        QCOMPARE(session.match().roundState(), 2);
        for (int i = 0; i < 30; ++i)
            session.tick({}, {});
        frame = renderer.render(session, true);
        QVERIFY(litPixels(frame) > 320 * 240 / 2);
        if (!out.isEmpty())
            frame.scaled(960, 720).save(out);

        const QString intro = QDir(engineDir).filePath("data/kfm/intro.def");
        if (QFileInfo::exists(intro)) {
            std::unique_ptr<Project> story(mugen.openProject(intro, &error));
            QVERIFY2(story, qPrintable(error));
            StoryboardPlayer player;
            QVERIFY2(player.start(*story, intro, engineDir, &error), qPrintable(error));
            QVERIFY(player.scenes().size() > 1);
            QImage mountains;
            QImage title;
            for (int i = 0; i < 2000 && !player.isFinished(); ++i) {
                player.tick();
                if (i == 150)
                    mountains = player.render();
                if (i == 480)
                    title = player.render();
            }
            QVERIFY(player.isFinished());
            QVERIFY(litPixels(mountains) > 0);
            if (!out.isEmpty()) {
                mountains.scaled(mountains.size() * 2).save(QString(out).replace(".png", "_story1.png"));
                title.scaled(title.size() * 2).save(QString(out).replace(".png", "_story2.png"));
            }
        }

        const QString system = QDir(engineDir).filePath("data/system.def");
        if (QFileInfo::exists(system)) {
            std::unique_ptr<Project> motif(mugen.openProject(system, &error));
            QVERIFY2(motif, qPrintable(error));
            QCOMPARE(motif->typeId(), QStringLiteral("motif"));
            const char* names[] = {"menu", "versus", "continue", "victory", "win", "survival"};
            for (int screen = 0; screen < 6; ++screen) {
                MotifScreen shown;
                QVERIFY2(shown.start(*motif, system, engineDir, MotifScreen::Screen(screen), &error),
                         qPrintable(error));
                Runtime::InputState down;
                down.down = true;
                for (int i = 0; i < 40; ++i)
                    shown.tick(screen == 0 && (i == 20 || i == 22) ? down : Runtime::InputState());
                if (screen == 0) {
                    QVERIFY(shown.menuItems().size() >= 5);
                    QCOMPARE(shown.menuIndex(), 2);
                }
                const QImage picture = shown.render();
                QVERIFY(litPixels(picture) > 0);
                if (!out.isEmpty())
                    picture.scaled(picture.size() * 2)
                        .save(
                            QString(out).replace(".png", QStringLiteral("_%1.png").arg(QLatin1String(names[screen]))));
            }
        }
    }
};

QTEST_MAIN(EmulatorTest)
#include "EmulatorTest.moc"
