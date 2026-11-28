// SPDX-License-Identifier: GPL-3.0-or-later
#include "UpdateRunner.h"
#include "Checksum.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>

namespace {
const char kHelperResource[] = ":/update/turbo-kate-update.sh";

QString tempUpdateDir()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    return QDir(base).filePath(QStringLiteral("turbo-update-%1")
                                   .arg(QCoreApplication::applicationPid()));
}
} // namespace

UpdateRunner::UpdateRunner(QObject* parent) : QObject(parent) {}

bool UpdateRunner::isAppImageRun()
{
    return !qEnvironmentVariableIsEmpty("APPIMAGE");
}

QString UpdateRunner::appImagePath()
{
    return qEnvironmentVariable("APPIMAGE");
}

bool UpdateRunner::prepareSwap(const QString& imagePath, const QString& expectedSha256,
                               QString* outHelperPath, QStringList* outArgs, QString* err)
{
    if (!Checksum::verifyFile(imagePath, expectedSha256)) {
        if (err)
            *err = QStringLiteral("checksum mismatch");
        return false;
    }
    if (!isAppImageRun()) {
        if (err)
            *err = QStringLiteral("not running as an AppImage");
        return false;
    }

    const QString dirPath = tempUpdateDir();
    if (!QDir().mkpath(dirPath)) {
        if (err)
            *err = QStringLiteral("could not create temp dir");
        return false;
    }
    const QString helper = QDir(dirPath).filePath(QStringLiteral("turbo-kate-update.sh"));
    QFile res(kHelperResource);
    if (!res.open(QIODevice::ReadOnly)) {
        if (err)
            *err = QStringLiteral("embedded helper missing");
        return false;
    }
    {
        QFile out(helper);
        if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            if (err)
                *err = QStringLiteral("could not write helper");
            return false;
        }
        out.write(res.readAll());
        out.close();
    }
    if (!QFile::setPermissions(helper,
                               QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner
                                   | QFile::ReadGroup | QFile::ExeGroup
                                   | QFile::ReadOther | QFile::ExeOther)) {
        if (err)
            *err = QStringLiteral("could not mark helper executable");
        return false;
    }

    if (outHelperPath)
        *outHelperPath = helper;
    if (outArgs)
        *outArgs = QStringList{appImagePath(), imagePath, expectedSha256};
    return true;
}

void UpdateRunner::start(const QUrl& appImageUrl, const QUrl& sha256Url)
{
    m_appImageUrl = appImageUrl;
    m_sha256Url = sha256Url;

    auto* nam = new QNetworkAccessManager(this);
    QNetworkRequest req(sha256Url);
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("TurboEditor1988-Updater"));
    req.setTransferTimeout(10000);
    QNetworkReply* shaReply = nam->get(req);
    connect(shaReply, &QNetworkReply::finished, this, [this, shaReply]() {
        shaReply->deleteLater();
        if (shaReply->error() != QNetworkReply::NoError) {
            emit failed(QStringLiteral("could not fetch checksum: %1")
                            .arg(shaReply->errorString()));
            return;
        }
        m_sha256Asset = shaReply->readAll();

        // Now fetch the AppImage itself.
        const QString dirPath = tempUpdateDir();
        QDir().mkpath(dirPath);
        m_downloadedImage = QDir(dirPath).filePath(QStringLiteral("TurboEditor1988-x86_64.AppImage"));
        auto* nam2 = new QNetworkAccessManager(this);
        QNetworkRequest req2(m_appImageUrl);
        req2.setHeader(QNetworkRequest::UserAgentHeader,
                       QStringLiteral("TurboEditor1988-Updater"));
        req2.setTransferTimeout(120000);
        QNetworkReply* imgReply = nam2->get(req2);
        auto* out = new QFile(m_downloadedImage, this);
        if (!out->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            emit failed(QStringLiteral("could not write download"));
            return;
        }
        connect(imgReply, &QNetworkReply::downloadProgress, this,
                &UpdateRunner::progress);
        connect(imgReply, &QNetworkReply::readyRead, this, [imgReply, out]() {
            out->write(imgReply->readAll());
        });
        connect(imgReply, &QNetworkReply::finished, this, [this, imgReply, out]() {
            imgReply->deleteLater();
            out->write(imgReply->readAll());
            out->close();
            if (imgReply->error() != QNetworkReply::NoError) {
                emit failed(QStringLiteral("download failed: %1")
                                .arg(imgReply->errorString()));
                return;
            }
            const QString want = QString::fromLatin1(m_sha256Asset).trimmed();
            QString err, helper;
            QStringList args;
            if (!prepareSwap(m_downloadedImage, want, &helper, &args, &err)) {
                emit failed(err);
                return;
            }
            emit swapAndRestart(helper, args);
        });
    });
}
