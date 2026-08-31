#include "core/app/Translations.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QLibraryInfo>
#include <QTemporaryDir>
#include <QTest>

using namespace OFS;

class TranslationsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void localeNames()
    {
        QCOMPARE(Translations::localeName("pt-br"), QStringLiteral("pt_BR"));
        QCOMPARE(Translations::localeName("DE"), QStringLiteral("de"));
    }

    void installAndRemove()
    {
        const QString qtGerman = QLibraryInfo::path(QLibraryInfo::TranslationsPath) + "/qtbase_de.qm";
        if (!QFile::exists(qtGerman))
            QSKIP("Qt's German translation is not installed");
        QCOMPARE(Translations::install("de-de", {}), 1);
        QCOMPARE(QCoreApplication::translate("QPlatformTheme", "Cancel"), QStringLiteral("Abbrechen"));
        QCOMPARE(Translations::install("en-us", {}), 0);
        QCOMPARE(QCoreApplication::translate("QPlatformTheme", "Cancel"), QStringLiteral("Cancel"));

        QTemporaryDir user;
        QTemporaryDir shared;
        QVERIFY(QDir(shared.path()).mkpath("languages/xx-yy"));
        QVERIFY(QFile::copy(qtGerman, shared.filePath("languages/xx-yy/ofs.qm")));
        QCOMPARE(Translations::install("xx-yy", {user.path(), shared.path()}), 1);
        QCOMPARE(QCoreApplication::translate("QPlatformTheme", "Cancel"), QStringLiteral("Abbrechen"));
        Translations::install("en-us", {});
    }
};

QTEST_GUILESS_MAIN(TranslationsTest)
#include "TranslationsTest.moc"
