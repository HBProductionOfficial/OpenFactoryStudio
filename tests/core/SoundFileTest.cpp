#include "core/sounds/SoundFile.h"
#include "core/sounds/SoundDocument.h"

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

QTEST_GUILESS_MAIN(SoundFileTest)
#include "SoundFileTest.moc"
