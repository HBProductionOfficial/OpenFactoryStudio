#pragma once

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

namespace OFS {

// Checks the project's GitHub releases for a newer version (Help > Check updates..., and at startup when "auto_update"
// is on). Nothing is downloaded or installed. The user gets a link to the release page.
class UpdateChecker : public QObject
{
    Q_OBJECT

public:
    explicit UpdateChecker(QObject* parent = nullptr);

    // `interactive` shows errors and "up to date" messages. Automatic checks stay silent unless a newer version exists.
    void check(bool interactive);

    // Compares dotted version strings ("1.2.10" > "1.2.9"). A leading 'v' is ignored.
    static int compareVersions(const QString& a, const QString& b);

    QString releasesUrl() const;

Q_SIGNALS:
    void updateAvailable(const QString& version, const QString& url, const QString& notes);
    void upToDate();
    void failed(const QString& message);

private:
    void onFinished(QNetworkReply* reply, bool interactive);

    QNetworkAccessManager* m_network;
};

} // namespace OFS
