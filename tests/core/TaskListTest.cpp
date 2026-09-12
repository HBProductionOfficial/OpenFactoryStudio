#include "core/app/TaskList.h"
#include "core/app/AppContext.h"
#include "core/sounds/SoundFile.h"
#include "core/sprites/SpriteFile.h"
#include "formats/audio/SampleBuffer.h"
#include "formats/snd/SndDocument.h"
#include "formats/wav/WavFile.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>

using namespace OFS;

namespace {

Formats::SffDocument oneSprite()
{
    SpriteDocument doc;
    Formats::Palette colors(256);
    colors[1] = Formats::Rgba{1, 2, 3, 255};
    doc.palettes() << SpriteDocument::makePalette(1, 1, colors);
    SpriteEntry sprite;
    sprite.pixels = SpriteDocument::makePixels(Formats::Image::indexed(4, 4, std::vector<std::uint8_t>(16, 1), colors));
    sprite.palette = doc.palettes().first();
    doc.sprites() << sprite;
    return doc.toSff();
}

} // namespace

class TaskListTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void tasks()
    {
        TaskList list;
        QSignalSpy changed(&list, &TaskList::changed);
        const int a = list.begin("Saving a");
        const int b = list.begin("Loading b");
        QCOMPARE(list.tasks().size(), 2);
        list.setProgress(a, 40);
        QCOMPARE(list.tasks().first().progress, 40);
        list.end(a);
        QCOMPARE(list.tasks().size(), 1);
        QCOMPARE(list.tasks().first().name, QStringLiteral("Loading b"));
        list.end(b);
        QVERIFY(list.isEmpty());
        QCOMPARE(changed.count(), 5);

        bool ran = false;
        list.run("quick", [&ran] { ran = true; });
        QVERIFY(ran);
        QCOMPARE(changed.count(), 5);
        list.run("slow", [] { QThread::msleep(400); });
        QCOMPARE(changed.count(), 7);
        QVERIFY(list.isEmpty());
    }

    void asyncSaves()
    {
        AppContext context;
        QTemporaryDir dir;
        const QString sff = dir.filePath("a.sff");
        QVERIFY(oneSprite().save(sff.toStdWString()).ok());
        SpriteFile sprites(sff, "sprites");
        QString error;
        QVERIFY2(sprites.load(&error), qPrintable(error));
        QVERIFY(sprites.canSaveAsync());
        SpriteDocument edited = sprites.document();
        edited.sprites()[0].header.axisX = 7;
        sprites.apply("axis", edited);
        QVERIFY(sprites.isModified());
        QSignalSpy saved(&sprites, &ProjectFile::saved);
        bool done = false;
        bool ok = false;
        sprites.saveAsync([&](bool result, const QString&) {
            done = true;
            ok = result;
        });
        QVERIFY(!context.tasks().isEmpty());
        QTRY_VERIFY(done);
        QVERIFY(ok);
        QCOMPARE(saved.count(), 1);
        QVERIFY(!sprites.isModified());
        QVERIFY(context.tasks().isEmpty());
        SpriteFile reread(sff, "sprites");
        QVERIFY(reread.load(&error));
        QCOMPARE(int(reread.document().sprites()[0].header.axisX), 7);

        edited.sprites()[0].header.axisX = 8;
        sprites.apply("axis", edited);
        done = false;
        sprites.saveAsync([&](bool, const QString&) { done = true; });
        edited.sprites()[0].header.axisX = 9;
        sprites.apply("axis", edited);
        QTRY_VERIFY(done);
        QVERIFY(sprites.isModified());
        QVERIFY(reread.load(&error));
        QCOMPARE(int(reread.document().sprites()[0].header.axisX), 8);

        Formats::SndDocument snd;
        Formats::SndSound sound;
        auto wav = Formats::WavFile::fromSamples(std::vector<double>(100, 0.25), 1, 22050, 16, false);
        QVERIFY(wav);
        sound.data = wav->write();
        snd.sounds.push_back(sound);
        const QString sndPath = dir.filePath("a.snd");
        QVERIFY(snd.save(sndPath.toStdWString()).ok());
        SoundFile sounds(sndPath, "sounds");
        QVERIFY(sounds.load(&error));
        SoundDocument changed = sounds.document();
        SoundEdits::setIdentifiers(changed, 0, 4, 2);
        sounds.apply("ids", changed);
        done = false;
        sounds.saveAsync([&](bool result, const QString&) {
            done = true;
            ok = result;
        });
        QTRY_VERIFY(done);
        QVERIFY(ok && !sounds.isModified());
        SoundFile again(sndPath, "sounds");
        QVERIFY(again.load(&error));
        QCOMPARE(again.document().sounds().first().group, 4);
    }
};

QTEST_MAIN(TaskListTest)
#include "TaskListTest.moc"
