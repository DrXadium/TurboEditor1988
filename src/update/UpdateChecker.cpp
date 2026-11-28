// SPDX-License-Identifier: GPL-3.0-or-later
#include "UpdateChecker.h"
#include "VersionCompare.h"
#include "src/AppIdentity.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace {
const QUrl kDefaultEndpoint(
    QStringLiteral("https://api.github.com/repos/DrXadium/TurboEditor1988/releases/latest"));

// Pull the download URL of a named release asset, or an empty URL.
QUrl assetUrl(const QJsonObject& release, const QString& name)
{
    for (const QJsonValue& v : release.value(QLatin1String("assets")).toArray()) {
        const QJsonObject a = v.toObject();
        if (a.value(QLatin1String("name")).toString() == name)
            return QUrl(a.value(QLatin1String("browser_download_url")).toString());
    }
    return {};
}
} // namespace

UpdateChecker::UpdateChecker(QObject* parent) : QObject(parent) {}

void UpdateChecker::setEndpoint(const QUrl& url)
{
    m_endpoint = url;
}

void UpdateChecker::check()
{
    const QUrl url = m_endpoint.isEmpty() ? kDefaultEndpoint : m_endpoint;
    auto* nam = new QNetworkAccessManager(this);
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("TurboEditor1988-Updater"));
    req.setTransferTimeout(10000);          // 10s, never hang the launch

    QNetworkReply* reply = nam->get(req);
    // nam/reply are children of this; reply cleans itself on finish.
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        const int status =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError || status < 200 || status >= 300) {
            emit checkFailed(reply->errorString());
            return;
        }
        QJsonParseError perr{};
        const QJsonDocument doc =
            QJsonDocument::fromJson(reply->readAll(), &perr);
        if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
            emit checkFailed(QStringLiteral("bad release JSON"));
            return;
        }
        const QJsonObject rel = doc.object();
        const QString tag = rel.value(QLatin1String("tag_name")).toString();
        if (tag.isEmpty()) {
            emit checkFailed(QStringLiteral("no tag_name"));
            return;
        }
        if (!VersionCompare::isNewer(tag, AppIdentity::version()))
            return;                          // equal/older: silently no update

        const QUrl appImg = assetUrl(rel, QStringLiteral("TurboEditor1988-x86_64.AppImage"));
        const QUrl sha = assetUrl(rel, QStringLiteral("TurboEditor1988-x86_64.AppImage.sha256"));
        if (appImg.isEmpty() || sha.isEmpty()) {
            emit checkFailed(QStringLiteral("release assets missing"));
            return;
        }
        QString version = tag;
        if (version.startsWith(QLatin1Char('v')))
            version.remove(0, 1);
        if (const int cut = version.indexOf(QLatin1Char('-')); cut >= 0)
            version.truncate(cut);
        emit updateAvailable(version, appImg, sha,
                             rel.value(QLatin1String("body")).toString());
    });
}
