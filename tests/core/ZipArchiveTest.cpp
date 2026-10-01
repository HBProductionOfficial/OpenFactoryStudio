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
};

QTEST_GUILESS_MAIN(ZipArchiveTest)
#include "ZipArchiveTest.moc"
