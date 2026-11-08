#include "shell/UpdateChecker.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
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

    // The tag is shown and the link opened, so only a version and this project's release page pass.
    void releaseReply()
    {
        UpdateChecker checker;
        QSignalSpy available(&checker, &UpdateChecker::updateAvailable);
        QSignalSpy failed(&checker, &UpdateChecker::failed);
        auto answer = [&checker](const QString& tag, const QString& url) {
            const QJsonObject release{{"tag_name", tag}, {"html_url", url}};
            UpdateChecker::setReplyOverride(
                [body = QJsonDocument(release).toJson()](bool) { return UpdateChecker::Reply{body, {}, 200}; });
            checker.check(true);
        };
        const QString page = checker.releasesUrl() + "/tag/v99.0.0";
        answer("v99.0.0", page);
        QTRY_COMPARE(available.size(), qsizetype(1));
        QCOMPARE(available[0][1].toString(), page);
        answer("v99.0.0", "https://example.com/setup.exe");
        QTRY_COMPARE(available.size(), qsizetype(2));
        QCOMPARE(available[1][1].toString(), checker.releasesUrl());
        answer("v99.0.0", checker.releasesUrl() + "/../../../someone/else");
        QTRY_COMPARE(available.size(), qsizetype(3));
        QCOMPARE(available[2][1].toString(), checker.releasesUrl());
        answer("v99.0.0", checker.releasesUrl() + "/%2e%2e/%2e%2e/someone/else");
        QTRY_COMPARE(available.size(), qsizetype(4));
        QCOMPARE(available[3][1].toString(), checker.releasesUrl());
        answer("<a href='x'>v99</a>", page);
        QTRY_COMPARE(failed.size(), qsizetype(1));
        answer("v99.0.0\n", page);
        QTRY_COMPARE(failed.size(), qsizetype(2));
        QCOMPARE(available.size(), qsizetype(4));
        UpdateChecker::setReplyOverride({});
    }
};

QTEST_GUILESS_MAIN(UpdateCheckerTest)
#include "UpdateCheckerTest.moc"
