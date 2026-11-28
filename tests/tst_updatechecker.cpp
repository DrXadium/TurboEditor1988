// SPDX-License-Identifier: GPL-3.0-or-later
// Task 3 (auto-update): UpdateChecker queries a release endpoint and emits
// updateAvailable ONLY when the release is newer than the built-in version.
// Network errors / HTTP errors / bad JSON must be silent no-ops.
//
// The endpoint is injected (setEndpoint) so tests drive a local QTcpServer
// stub — no live GitHub hits.
#include <QtTest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QSignalSpy>
#include "src/update/UpdateChecker.h"
#include "src/AppIdentity.h"

namespace {
// A one-shot HTTP stub: replies to the first GET with the given status+body.
class FakeReleaseServer : public QTcpServer
{
    Q_OBJECT
public:
    int status = 200;
    QByteArray body;
    void incomingConnection(qintptr fd) override
    {
        auto* sock = new QTcpSocket(this);
        sock->setSocketDescriptor(fd);
        connect(sock, &QTcpSocket::readyRead, this, [this, sock]() {
            const QByteArray req = sock->readAll();
            if (!req.startsWith("GET "))
                return;
            const QByteArray hdr =
                "HTTP/1.1 " + QByteArray::number(status) + " X\r\n"
                "Content-Type: application/json\r\n"
                "Content-Length: " + QByteArray::number(body.size()) + "\r\n"
                "Connection: close\r\n\r\n" + body;
            sock->write(hdr);
            sock->disconnectFromHost();
        });
    }
};

QByteArray releaseJson(const QString& tag, const QString& appImg, const QString& sha)
{
    return QStringLiteral(
        "{\"tag_name\":\"%1\",\"body\":\"notes\","
        "\"assets\":["
        "{\"name\":\"TurboEditor1988-x86_64.AppImage\",\"browser_download_url\":\"%2\"},"
        "{\"name\":\"TurboEditor1988-x86_64.AppImage.sha256\",\"browser_download_url\":\"%3\"}"
        "]}")
        .arg(tag, appImg, sha)
        .toUtf8();
}
} // namespace

class TstUpdatechecker : public QObject
{
    Q_OBJECT
private slots:
    void newer_release_emits_updateAvailable()
    {
        FakeReleaseServer srv;
        srv.body = releaseJson(QStringLiteral("v9.9.9"),
                               QStringLiteral("http://x/a.AppImage"),
                               QStringLiteral("http://x/a.sha256"));
        QVERIFY(srv.listen(QHostAddress::LocalHost, 0));

        UpdateChecker c;
        c.setEndpoint(QUrl(QStringLiteral("http://127.0.0.1:%1/").arg(srv.serverPort())));
        QSignalSpy spy(&c, &UpdateChecker::updateAvailable);
        c.check();
        QVERIFY(spy.wait(3000));
        QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("9.9.9"));
        QCOMPARE(spy.at(0).at(1).toUrl().toString(), QStringLiteral("http://x/a.AppImage"));
        QCOMPARE(spy.at(0).at(2).toUrl().toString(), QStringLiteral("http://x/a.sha256"));
    }

    void equal_release_does_not_emit()
    {
        FakeReleaseServer srv;
        // Same as the built-in version -> not an update.
        srv.body = releaseJson(QStringLiteral("v") + AppIdentity::version(),
                               QStringLiteral("http://x/a.AppImage"),
                               QStringLiteral("http://x/a.sha256"));
        QVERIFY(srv.listen(QHostAddress::LocalHost, 0));

        UpdateChecker c;
        c.setEndpoint(QUrl(QStringLiteral("http://127.0.0.1:%1/").arg(srv.serverPort())));
        QSignalSpy spy(&c, &UpdateChecker::updateAvailable);
        c.check();
        // Wait for the reply to be processed, then assert silence.
        QVERIFY(!spy.wait(1500));
        QCOMPARE(spy.count(), 0);
    }

    void network_error_is_silent_no_op()
    {
        UpdateChecker c;
        // Nothing listening on this port.
        c.setEndpoint(QUrl(QStringLiteral("http://127.0.0.1:1/")));
        QSignalSpy spy(&c, &UpdateChecker::updateAvailable);
        QSignalSpy failSpy(&c, &UpdateChecker::checkFailed);
        c.check();
        QVERIFY(!spy.wait(1500));
        QCOMPARE(spy.count(), 0);
        // A failure signal is fine (informative), but it must not crash.
        Q_UNUSED(failSpy);
    }
};

QTEST_MAIN(TstUpdatechecker)
#include "tst_updatechecker.moc"
