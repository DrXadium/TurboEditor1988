// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QObject>
#include <QUrl>

// Async check for a newer TurboEditor1988 release on GitHub.
//
// check() fires one GET at a release endpoint and never blocks the caller.
// It emits updateAvailable only when the release tag is newer than the built-in
// AppIdentity::version(); an equal/older release, a network error, a non-200
// status, or unparseable JSON are all silent (checkFailed is informational
// only) — the app must start and work exactly the same whether or not the
// check succeeds. The endpoint is injectable so tests drive a local stub.
class UpdateChecker : public QObject
{
    Q_OBJECT
public:
    explicit UpdateChecker(QObject* parent = nullptr);

    // Test seam. Empty => the real GitHub releases/latest URL.
    void setEndpoint(const QUrl& url);

    // Fire the check. Async; returns immediately.
    void check();

signals:
    // A release newer than the running build. version is a bare X.Y.Z.
    void updateAvailable(const QString& version, const QUrl& appImageUrl,
                         const QUrl& sha256Url, const QString& notes);
    // Informational: the check could not run or the payload was unusable.
    void checkFailed(const QString& reason);

private:
    QUrl m_endpoint;
};
