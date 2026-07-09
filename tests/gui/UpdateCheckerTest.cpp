#include "shell/UpdateChecker.h"

#include <QTest>

using namespace OFS;

class UpdateCheckerTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void compareVersions()
    {
        QCOMPARE(UpdateChecker::compareVersions("1.2.10", "1.2.9"), 1);
        QCOMPARE(UpdateChecker::compareVersions("v0.1.0", "0.1"), 0);
        QCOMPARE(UpdateChecker::compareVersions("0.9.9", "1.0.0"), -1);
        QCOMPARE(UpdateChecker::compareVersions("v1.0.0-beta", "1.0.0"), 0);
        QCOMPARE(UpdateChecker::compareVersions("2", "1.99.99"), 1);
    }
};

QTEST_GUILESS_MAIN(UpdateCheckerTest)
#include "UpdateCheckerTest.moc"
