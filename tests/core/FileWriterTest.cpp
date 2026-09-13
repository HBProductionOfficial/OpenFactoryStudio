#include "core/project/FileWriter.h"

#include <QFile>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QTest>

#include <chrono>
#include <thread>

using namespace OFS;

namespace {

QByteArray readAll(const QString& path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

} // namespace

class FileWriterTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void missingFolder()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("sub/folder/a.txt");
        QString error;
        QVERIFY2(writeFileSafely(path, "data", &error), qPrintable(error));
        QCOMPARE(readAll(path), QByteArray("data"));
    }

    // A reader holding the file for a moment must not make the save fail.
    void briefReader()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("a.txt");
        QVERIFY(writeFileSafely(path, "old"));
        QFile reader(path);
        QVERIFY(reader.open(QIODevice::ReadOnly));
#if defined(Q_OS_WIN)
        // Why the writer tries again: the file cannot be replaced while it is open.
        {
            QSaveFile plain(path);
            QVERIFY(plain.open(QIODevice::WriteOnly));
            plain.write("x");
            QVERIFY(!plain.commit());
        }
#endif
        std::thread closer([&reader] {
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
            reader.close();
        });
        QString error;
        const bool written = writeFileSafely(path, "new", &error);
        closer.join();
        QVERIFY2(written, qPrintable(error));
        QCOMPARE(readAll(path), QByteArray("new"));
    }
};

QTEST_GUILESS_MAIN(FileWriterTest)
#include "FileWriterTest.moc"
