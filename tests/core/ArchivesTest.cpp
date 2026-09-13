#include "core/project/Archives.h"
#include "core/app/Paths.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

using namespace OFS;

namespace {

void put16(QByteArray& b, quint16 v)
{
    b.append(char(v & 0xff)).append(char(v >> 8));
}

void put32(QByteArray& b, quint32 v)
{
    put16(b, quint16(v & 0xffff));
    put16(b, quint16(v >> 16));
}

quint32 crc(const QByteArray& data)
{
    quint32 c = 0xffffffffu;
    for (char byte : data) {
        c ^= quint8(byte);
        for (int k = 0; k < 8; ++k)
            c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1u)));
    }
    return ~c;
}

// A RAR block: its CRC (the low half of the CRC-32 of what follows) in front.
QByteArray block(const QByteArray& body)
{
    QByteArray b;
    put16(b, quint16(crc(body) & 0xffff));
    return b + body;
}

// A name ending in '/' is a folder.
QByteArray storedRar(const QList<QPair<QByteArray, QByteArray>>& files)
{
    QByteArray rar("Rar!\x1a\x07\x00", 7);
    QByteArray main;
    main.append(char(0x73));
    put16(main, 0);
    put16(main, 13);
    put16(main, 0);
    put32(main, 0);
    rar += block(main);
    for (const auto& [name, data] : files) {
        const bool folder = name.endsWith('/');
        const QByteArray stored = folder ? name.chopped(1) : name;
        QByteArray file;
        file.append(char(0x74));
        put16(file, quint16(0x8000 | (folder ? 0x00e0 : 0))); // data size field; folder
        put16(file, quint16(32 + stored.size()));
        put32(file, quint32(data.size())); // packed
        put32(file, quint32(data.size())); // unpacked
        file.append(char(2));              // Windows
        put32(file, crc(data));
        put32(file, 0x5a210000); // 2025-01-01 in DOS time
        file.append(char(20));   // version needed
        file.append(char(0x30)); // stored
        put16(file, quint16(stored.size()));
        put32(file, folder ? 0x10 : 0x20);
        file += stored;
        rar += block(file) + data;
    }
    QByteArray end;
    end.append(char(0x7b));
    put16(end, 0x4000);
    put16(end, 7);
    return rar + block(end);
}

void writeFile(const QString& path, const QByteArray& data)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

} // namespace

class ArchivesTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase() { Archives::loadExtensions(Paths::extensionSearchDirs()); }

    void zipIsBuiltIn()
    {
        QVERIFY(Archives::formatOf("pack.ZIP"));
        QVERIFY(!Archives::formatOf("kfm.def"));
        QString error;
        QVERIFY(Archives::entries("kfm.def", &error).isEmpty());
        QVERIFY(!error.isEmpty());
    }

    void rar()
    {
        if (!Archives::formatOf("x.rar"))
            QSKIP("built without the RAR extension");
        QCOMPARE(Archives::openFilter(),
                 QStringLiteral("All supported (*.rar *.zip);;Roshal ARchive (*.rar);;ZIP Archive (*.zip)"));
        QTemporaryDir dir;
        const QString path = dir.filePath("kfm.RAR");
        writeFile(path, storedRar({{"kfm/", {}},
                                   {"kfm/kfm.def", "[Info]\nname = \"Kung Fu Man\"\n"},
                                   {"kfm\\sub\\kfm.cns", "[Data]\nlife = 1000\n"}}));
        QString error;
        const QList<Zip::Entry> entries = Archives::entries(path, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(entries.size(), 3);
        QCOMPARE(entries[0].name, QStringLiteral("kfm"));
        QVERIFY(entries[0].directory);
        QCOMPARE(entries[1].name, QStringLiteral("kfm/kfm.def"));
        QCOMPARE(entries[1].size, 28);
        QCOMPARE(entries[2].name, QStringLiteral("kfm/sub/kfm.cns"));
        QCOMPARE(Archives::read(path, "kfm/sub/kfm.cns", &error), QByteArray("[Data]\nlife = 1000\n"));
        QVERIFY(Archives::read(path, "kfm/none.txt", &error).isEmpty());
        QVERIFY(error.contains("kfm/none.txt"));

        const QString out = dir.filePath("out");
        QVERIFY2(Archives::extract(path, out, &error), qPrintable(error));
        QFile cns(out + "/kfm/sub/kfm.cns");
        QVERIFY(cns.open(QIODevice::ReadOnly));
        QCOMPARE(cns.readAll(), QByteArray("[Data]\nlife = 1000\n"));

        const QString evil = dir.filePath("evil.rar");
        writeFile(evil, storedRar({{"../evil.txt", "x"}}));
        QVERIFY(!Archives::extract(evil, dir.filePath("out2"), &error));
        QVERIFY(error.contains("outside"));
        QVERIFY(!QFile::exists(dir.filePath("evil.txt")));

        writeFile(dir.filePath("bad.rar"), "not an archive");
        QVERIFY(Archives::entries(dir.filePath("bad.rar"), &error).isEmpty());
        QVERIFY(!error.isEmpty());
    }

    void rarFile()
    {
        const QString path = qEnvironmentVariable("OFS_TEST_RAR");
        if (path.isEmpty() || !Archives::formatOf(path))
            QSKIP("OFS_TEST_RAR is not set (or built without the RAR extension)");
        QString error;
        const QList<Zip::Entry> entries = Archives::entries(path, &error);
        QVERIFY2(!entries.isEmpty(), qPrintable(error));
        QTemporaryDir dir;
        QVERIFY2(Archives::extract(path, dir.path(), &error), qPrintable(error));
        for (const Zip::Entry& entry : entries) {
            if (!entry.directory)
                QCOMPARE(QFileInfo(dir.filePath(entry.name)).size(), entry.size);
        }
        qInfo() << entries.size() << "entries extracted";
    }
};

QTEST_GUILESS_MAIN(ArchivesTest)
#include "ArchivesTest.moc"
