#include "core/project/ZipArchive.h"

#include <miniz.h>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace OFS;

namespace {

void writeFile(const QString& path, const QByteArray& data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

QByteArray readFile(const QString& path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

} // namespace

class ZipArchiveTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void roundTrip()
    {
        QTemporaryDir dir;
        const QString source = dir.filePath("src");
        const QString binary = QStringLiteral("binary.sff");
        const QString accented = QString::fromUtf8("sub/d\xc3\xa9j\xc3\xa0.cns"); // déjà
        QByteArray bytes;
        for (int i = 0; i < 5000; ++i)
            bytes += char(i * 7);
        writeFile(source + "/kfm.def", "[Info]\nname = \"kfm\"\n");
        writeFile(source + "/" + binary, bytes);
        writeFile(source + "/" + accented, "[Data]\nlife = 1000\n");
        const QString archive = dir.filePath("p.zip");
        QString error;
        QVERIFY2(Zip::create(archive, source, {"kfm.def", binary, accented}, &error), qPrintable(error));

        const QList<Zip::Entry> list = Zip::entries(archive, &error);
        QCOMPARE(list.size(), 3);
        QCOMPARE(list[0].name, QStringLiteral("kfm.def"));
        QCOMPARE(list[1].size, qint64(5000));
        QCOMPARE(list[2].name, accented);
        QCOMPARE(Zip::read(archive, binary), bytes);

        const QString target = dir.filePath("out");
        QVERIFY2(Zip::extract(archive, target, &error), qPrintable(error));
        QCOMPARE(readFile(target + "/" + binary), bytes);
        QCOMPARE(readFile(target + "/" + accented), QByteArray("[Data]\nlife = 1000\n"));

        QVERIFY(Zip::entries(source + "/kfm.def", &error).isEmpty());
        QVERIFY(!error.isEmpty());
    }

    void outsideTheFolder()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("evil.txt"), "x");
        QDir().mkpath(dir.filePath("root"));
        const QString archive = dir.filePath("bad.zip");
        QVERIFY(Zip::create(archive, dir.filePath("root"), {"../evil.txt"}));
        QFile::remove(dir.filePath("evil.txt"));
        QString error;
        QVERIFY(!Zip::extract(archive, dir.filePath("out/inner"), &error));
        QVERIFY(!QFile::exists(dir.filePath("out/evil.txt")));
        QVERIFY(!error.isEmpty());
    }

    void backslashNames()
    {
        // Backslash folders, as Windows PowerShell writes them.
        QTemporaryDir dir;
        const QString archive = dir.filePath("ps.zip");
        mz_zip_archive zip{};
        QVERIFY(mz_zip_writer_init_file(&zip, archive.toLocal8Bit().constData(), 0));
        QVERIFY(mz_zip_writer_add_mem(&zip, "kfm\\", nullptr, 0, mz_uint(MZ_DEFAULT_LEVEL)));
        QVERIFY(mz_zip_writer_add_mem(&zip, "kfm\\kfm.def", "[Info]", 6, mz_uint(MZ_DEFAULT_LEVEL)));
        QVERIFY(mz_zip_writer_finalize_archive(&zip));
        mz_zip_writer_end(&zip);

        const QList<Zip::Entry> list = Zip::entries(archive);
        QCOMPARE(list.size(), qsizetype(2));
        QCOMPARE(list[0].name, QStringLiteral("kfm/"));
        QVERIFY(list[0].directory);
        QCOMPARE(list[1].name, QStringLiteral("kfm/kfm.def"));
        QCOMPARE(Zip::read(archive, "kfm/kfm.def"), QByteArray("[Info]"));
        QVERIFY(Zip::extract(archive, dir.filePath("out")));
        QCOMPARE(readFile(dir.filePath("out/kfm/kfm.def")), QByteArray("[Info]"));
    }

    void otherLetterCase()
    {
#ifndef Q_OS_LINUX
        QSKIP("Folder names ignore letter case here.");
#else
        // "OUT" is another folder next to "out".
        QTemporaryDir dir;
        const QString archive = dir.filePath("case.zip");
        mz_zip_archive zip{};
        QVERIFY(mz_zip_writer_init_file(&zip, archive.toLocal8Bit().constData(), 0));
        QVERIFY(mz_zip_writer_add_mem(&zip, "../OUT/evil.txt", "x", 1, mz_uint(MZ_DEFAULT_LEVEL)));
        QVERIFY(mz_zip_writer_finalize_archive(&zip));
        mz_zip_writer_end(&zip);
        QString error;
        QVERIFY(!Zip::extract(archive, dir.filePath("out"), &error));
        QVERIFY(!QFile::exists(dir.filePath("OUT/evil.txt")));
#endif
    }

    void driveOrStreamNames()
    {
#ifndef Q_OS_WIN
        QSKIP("A colon is an ordinary character in names here.");
#else
        // On Windows "a.txt:x" is a hidden stream of a.txt, and "C:x" a path on drive C.
        QTemporaryDir dir;
        for (const char* name : {"a.txt:x", "C:x.txt"}) {
            const QString archive = dir.filePath("colon.zip");
            QFile::remove(archive);
            mz_zip_archive zip{};
            QVERIFY(mz_zip_writer_init_file(&zip, archive.toLocal8Bit().constData(), 0));
            QVERIFY(mz_zip_writer_add_mem(&zip, name, "x", 1, mz_uint(MZ_DEFAULT_LEVEL)));
            QVERIFY(mz_zip_writer_finalize_archive(&zip));
            mz_zip_writer_end(&zip);
            QString error;
            QVERIFY2(!Zip::extract(archive, dir.filePath("out"), &error), name);
            QVERIFY(!QFile::exists(dir.filePath("out/a.txt")));
        }
#endif
    }

    void tooBigToUnpack()
    {
        QCOMPARE(Zip::extractLimit(0), qint64(2) << 30);
        QCOMPARE(Zip::extractLimit(qint64(100) << 20), qint64(5000) << 20);

        // An archive that says it unpacks to 4 GB is refused before anything is written.
        QTemporaryDir dir;
        writeFile(dir.filePath("root/a.txt"), QByteArray(1000, 'a'));
        const QString archive = dir.filePath("big.zip");
        QVERIFY(Zip::create(archive, dir.filePath("root"), {"a.txt"}));
        QByteArray bytes = readFile(archive);
        auto put32 = [&bytes](qsizetype at, quint32 value) {
            QVERIFY(at >= 0 && at + 4 <= bytes.size());
            for (int i = 0; i < 4; ++i)
                bytes[at + i] = char((value >> (8 * i)) & 0xFF);
        };
        // The unpacked size in the local header and in the central directory.
        put32(bytes.indexOf("PK\x03\x04") + 22, 0xF0000000u);
        put32(bytes.indexOf("PK\x01\x02") + 24, 0xF0000000u);
        writeFile(archive, bytes);
        QString error;
        QVERIFY(!Zip::extract(archive, dir.filePath("out"), &error));
        QVERIFY2(error.contains(QStringLiteral("big.zip")), qPrintable(error));
        QVERIFY(!QFile::exists(dir.filePath("out/a.txt")));
    }
};

QTEST_GUILESS_MAIN(ZipArchiveTest)
#include "ZipArchiveTest.moc"
