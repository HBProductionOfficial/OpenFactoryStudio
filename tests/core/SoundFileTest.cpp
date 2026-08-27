#include "core/sounds/SoundFile.h"
#include "core/sounds/SoundDocument.h"
#include "core/sounds/SoundTransfer.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QUndoStack>

using namespace OFS;
using namespace OFS::Formats;

namespace {

Bytes wav(std::size_t frames, std::int16_t value)
{
    const SampleBuffer buffer = SampleBuffer::fromInt16(std::vector<std::int16_t>(frames, value), 1, 11025);
    auto bytes = encodeAudio(buffer);
    return bytes ? bytes.value() : Bytes();
}

QByteArray toBytes(const Bytes& bytes)
{
    return QByteArray(reinterpret_cast<const char*>(bytes.data()), qsizetype(bytes.size()));
}

} // namespace

class SoundFileTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void documentAndEdits()
    {
        SndDocument snd;
        snd.sounds.push_back(SndSound{0, 0, wav(100, 1000)});
        snd.sounds.push_back(SndSound{5, 2, wav(50, -2000)});
        SoundDocument doc = SoundDocument::fromSnd(snd);
        QCOMPARE(doc.count(), 2);
        QCOMPARE(doc.find(5, 2), 1);
        QCOMPARE(doc.find(5, 3), -1);
        QCOMPARE(doc.toSnd().write(), snd.write());

        auto decoded = SoundEdits::decode(doc.sounds().at(0));
        QVERIFY(decoded.ok());
        QCOMPARE(decoded.value().buffer.frameCount(), std::size_t(100));
        QCOMPARE(decoded.value().bitsPerSample, std::uint16_t(16));
        SampleBuffer louder = decoded.value().buffer;
        louder.setSample(0, 0, 0.5f);
        QVERIFY(SoundEdits::setSamples(doc, 0, louder, 16));
        QCOMPARE(SoundEdits::decode(doc.sounds().at(0)).value().buffer.sample(0, 0), 0.5f);

        SoundEdits::setIdentifiers(doc, 1, 7, 1);
        QCOMPARE(doc.sounds().at(1).group, 7);
        QCOMPARE(SoundEdits::duplicate(doc, 0), 1);
        QCOMPARE(doc.sounds().at(1).data, doc.sounds().at(0).data);
        QCOMPARE(SoundEdits::insert(doc, -1, 9, 9, wav(10, 0)), 3);
        SoundEdits::remove(doc, {0, 1});
        QCOMPARE(doc.count(), 2);
        QCOMPARE(doc.sounds().at(0).group, 7);

        const Bytes original = wav(20, 300);
        QCOMPARE(SoundEdits::importData(original), original);
        EncodeOptions flac;
        flac.format = AudioFileFormat::Flac;
        auto flacBytes = encodeAudio(SampleBuffer::fromInt16(std::vector<std::int16_t>(20, 300), 1, 11025), flac);
        QVERIFY(flacBytes.ok());
        const Bytes imported = SoundEdits::importData(flacBytes.value());
        QCOMPARE(detectAudioFormat(ByteSpan(imported.data(), imported.size())), AudioFileFormat::Wav);
    }

    void file()
    {
        QTemporaryDir dir;
        SndDocument snd;
        snd.versionBytes = {1, 0, 1, 0};
        snd.sounds.push_back(SndSound{1, 1, wav(30, 500)});
        const QString path = dir.filePath("s.snd");
        {
            QFile f(path);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(toBytes(snd.write()));
        }
        SoundFile file(path, "sounds");
        QString error;
        QVERIFY2(file.load(&error), qPrintable(error));
        QCOMPARE(file.document().count(), 1);
        QVERIFY(!file.isModified());

        SoundDocument edited = file.document();
        SoundEdits::setIdentifiers(edited, 0, 2, 2);
        file.apply("Identifiers", edited);
        QVERIFY(file.isModified());
        QCOMPARE(file.document().sounds().at(0).group, 2);
        file.undoStack()->undo();
        QVERIFY(!file.isModified());
        QCOMPARE(file.document().sounds().at(0).group, 1);

        QVERIFY(file.writeCopy(dir.filePath("copy.snd"), &error));
        QFile copy(dir.filePath("copy.snd"));
        QVERIFY(copy.open(QIODevice::ReadOnly));
        QCOMPARE(copy.readAll(), toBytes(snd.write()));
        copy.close();
        file.undoStack()->redo();
        QVERIFY(file.saveAs(dir.filePath("saved.snd"), &error));
        QVERIFY(!file.isModified());
        SoundFile reread(dir.filePath("saved.snd"), "sounds");
        QVERIFY(reread.load(&error));
        QCOMPARE(reread.document().sounds().at(0).group, 2);
        QCOMPARE(reread.document().toSnd().versionBytes, snd.versionBytes);
    }
};

class SoundTransferTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void exportAndImport()
    {
        QTemporaryDir dir;
        SndDocument snd;
        snd.sounds.push_back(SndSound{0, 0, wav(10, 1)});
        snd.sounds.push_back(SndSound{900, 1, wav(20, 2)});
        const SoundDocument doc = SoundDocument::fromSnd(snd);
        QVERIFY(exportSounds(doc, {0, 1}, dir.filePath("list.ffe"), SoundListFormat::Ffe));
        QFile ffe(dir.filePath("list.ffe"));
        QVERIFY(ffe.open(QIODevice::ReadOnly));
        QCOMPARE(ffe.readAll(), QByteArray("\n[SoundDef]\ngroup = 0\nsample = 0\nfile = 0-0.wav\n"
                                           "\n[SoundDef]\ngroup = 900\nsample = 1\nfile = 900-1.wav\n"));
        QFile stored(dir.filePath("900-1.wav"));
        QVERIFY(stored.open(QIODevice::ReadOnly));
        QCOMPARE(stored.readAll(), toBytes(wav(20, 2)));
        stored.close();
        QVERIFY(exportSounds(doc, {1}, dir.filePath("g900.txt"), SoundListFormat::SndMaker));
        QFile txt(dir.filePath("g900.txt"));
        QVERIFY(txt.open(QIODevice::ReadOnly));
        QCOMPARE(txt.readAll(), QByteArray("g900\n900-1.wav\n900\n1\n"));

        SoundDocument target = doc;
        SoundEdits::remove(target, {0});
        QList<QPair<int, int>> asked;
        SoundImportOptions options;
        options.askConflict = [&asked](int group, int number) {
            asked << qMakePair(group, number);
            return SoundConflict::Replace;
        };
        QString error;
        const QList<int> added = importFfe(target, 0, dir.filePath("list.ffe"), options, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(added, QList<int>{1});
        QCOMPARE(asked, (QList<QPair<int, int>>{{900, 1}}));
        QCOMPARE(target.count(), 2);
    }

    void addAndSave()
    {
        QTemporaryDir dir;
        auto write = [&dir](const QString& name, const Bytes& data) {
            QFile f(dir.filePath(name));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(toBytes(data));
        };
        write("700_5.wav", wav(10, 3));
        write("plain.wav", wav(10, 4));
        write("plain2.wav", wav(10, 5));
        SoundDocument doc;
        SoundAddOptions options;
        options.detect = true;
        options.group = 900;
        options.number = 0;
        options.step = 10;
        QStringList errors;
        const QList<int> added =
            addSoundFiles(doc, -1, {dir.filePath("700_5.wav"), dir.filePath("plain.wav"), dir.filePath("plain2.wav")},
                          options, &errors);
        QVERIFY(errors.isEmpty());
        QCOMPARE(added, (QList<int>{0, 1, 2}));
        QCOMPARE(doc.sounds().at(0).group, 700);
        QCOMPARE(doc.sounds().at(0).number, 5);
        QCOMPARE(doc.sounds().at(1).number, 0);
        QCOMPARE(doc.sounds().at(2).number, 10);

        QString error;
        QCOMPARE(saveSounds(doc, {1}, dir.filePath("one.wav"), &error).size(), 1);
        QFile one(dir.filePath("one.wav"));
        QVERIFY(one.open(QIODevice::ReadOnly));
        QCOMPARE(one.readAll(), toBytes(wav(10, 4)));
        SoundDocument longer;
        SoundEdits::insert(longer, -1, 1, 1, wav(22050, 1000));
        QCOMPARE(saveSounds(longer, {0}, dir.filePath("long.ogg"), &error).size(), 1);
        auto ogg = loadAudio(dir.filePath("long.ogg").toStdString());
        QVERIFY(ogg.ok());
        QCOMPARE(ogg.value().buffer.frameCount(), std::size_t(22050));
        const QStringList several = saveSounds(doc, {0, 2}, dir.filePath("many.flac"), &error);
        QCOMPARE(several, (QStringList{dir.filePath("many_700-5.flac"), dir.filePath("many_900-10.flac")}));
    }
};

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    SoundFileTest file;
    SoundTransferTest transfer;
    return QTest::qExec(&file, argc, argv) | QTest::qExec(&transfer, argc, argv);
}
#include "SoundFileTest.moc"
