#include "core/app/AppContext.h"
#include "core/project/Project.h"
#include "core/project/ProjectFile.h"
#include "engines/mugen/MugenEngine.h"
#include "engines/mugen/emulator/EmulatorRenderer.h"
#include "engines/mugen/emulator/EmulatorSession.h"
#include "engines/mugen/emulator/EmulatorWidget.h"
#include "fixtures/FixtureFactory.h"
#include "formats/codecs/Pcx.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QTextCursor>
#include <QTextDocument>

#include <cmath>
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
        QCOMPARE(emulator.statusSections().value(2), QStringLiteral("Renderer: OpenGL"));
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
    }
};

QTEST_MAIN(EmulatorTest)
#include "EmulatorTest.moc"
