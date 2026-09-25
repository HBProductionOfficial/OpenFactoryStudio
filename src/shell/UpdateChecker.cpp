#include "shell/UpdateChecker.h"

#include "core/Version.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStringList>
#include <QTimer>

namespace OFS {

namespace {
UpdateChecker::ReplyOverride& replyOverride()
{
    static UpdateChecker::ReplyOverride answer;
    return answer;
}
} // namespace

void UpdateChecker::setReplyOverride(ReplyOverride answer)
{
    replyOverride() = std::move(answer);
}

UpdateChecker::UpdateChecker(QObject* parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{}

QString UpdateChecker::releasesUrl() const
{
    return QStringLiteral("https://github.com/%1/releases").arg(QStringLiteral(OFS_REPOSITORY));
}

void UpdateChecker::check(bool interactive)
{
    if (const ReplyOverride& answer = replyOverride()) {
        // Answer later, like the network would.
        const Reply reply = answer(interactive);
        QTimer::singleShot(0, this, [this, reply, interactive] { handle(reply, interactive); });
        return;
    }
    QNetworkRequest request(
        QUrl(QStringLiteral("https://api.github.com/repos/%1/releases/latest").arg(QStringLiteral(OFS_REPOSITORY))));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("OpenFactoryStudio/%1").arg(QStringLiteral(OFS_VERSION_STRING)));
    request.setTransferTimeout(15000);
    QNetworkReply* reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, interactive] { onFinished(reply, interactive); });
}

void UpdateChecker::onFinished(QNetworkReply* reply, bool interactive)
{
    reply->deleteLater();
    Reply answer;
    answer.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->error() != QNetworkReply::NoError)
        answer.error = reply->errorString();
    else
        answer.body = reply->readAll();
    handle(answer, interactive);
}

void UpdateChecker::handle(const Reply& reply, bool interactive)
{
    // The automatic check at startup only shows a newer version.
    if (!reply.error.isEmpty()) {
        if (interactive)
            Q_EMIT failed(reply.status == 404 ? tr("No release has been published yet.") : reply.error);
        return;
    }
    const QJsonObject release = QJsonDocument::fromJson(reply.body).object();
    const QString tag = release.value(QStringLiteral("tag_name")).toString();
    if (tag.isEmpty()) {
        if (interactive)
            Q_EMIT failed(tr("The release information could not be read."));
        return;
    }
    if (compareVersions(tag, QStringLiteral(OFS_VERSION_STRING)) > 0) {
        QString url = release.value(QStringLiteral("html_url")).toString();
        if (url.isEmpty())
            url = releasesUrl();
        Q_EMIT updateAvailable(tag, url, release.value(QStringLiteral("body")).toString());
    } else if (interactive) {
        Q_EMIT upToDate();
    }
}

int UpdateChecker::compareVersions(const QString& a, const QString& b)
{
    auto parts = [](QString v) {
        v = v.trimmed();
        if (v.startsWith(QLatin1Char('v'), Qt::CaseInsensitive))
            v = v.mid(1);
        v = v.section(QLatin1Char('-'), 0, 0);
        QList<int> numbers;
        for (const QString& p : v.split(QLatin1Char('.')))
            numbers << p.toInt();
        return numbers;
    };
    const QList<int> x = parts(a);
    const QList<int> y = parts(b);
    for (qsizetype i = 0; i < qMax(x.size(), y.size()); ++i) {
        const int xi = i < x.size() ? x[i] : 0;
        const int yi = i < y.size() ? y[i] : 0;
        if (xi != yi)
            return xi < yi ? -1 : 1;
    }
    return 0;
}

} // namespace OFS
