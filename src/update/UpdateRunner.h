// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QObject>
#include <QStringList>
#include <QUrl>

// Drives one update: download the new AppImage + .sha256, verify the checksum,
// then stage the detached swap helper and hand control to it (the caller quits
// so the helper can replace the file only after this process is gone).
//
// Nothing here ever touches the installed file: prepareSwap only writes the
// helper into a temp dir. The actual swap is the helper's job (Task 7).
class UpdateRunner : public QObject
{
    Q_OBJECT
public:
    explicit UpdateRunner(QObject* parent = nullptr);

    // True when running as an AppImage (the runtime sets APPIMAGE to the
    // running file's path). Only then is there a file to swap.
    static bool isAppImageRun();
    static QString appImagePath();

    // Download both assets to a temp dir, verify the AppImage's SHA-256 against
    // the .sha256 asset, then stage the helper. On success emits swapAndRestart
    // with the helper path + args (target, new image, expected sha); on any
    // failure emits failed(reason) and touches nothing.
    void start(const QUrl& appImageUrl, const QUrl& sha256Url);

    // Verify a local payload against a hex digest and, if it matches, write the
    // embedded helper into a temp dir. Returns false (with *err set) on a
    // mismatch or a write failure; on success fills *outHelperPath and *outArgs.
    // Static and synchronous so tests can drive it directly.
    static bool prepareSwap(const QString& imagePath, const QString& expectedSha256,
                            QString* outHelperPath, QStringList* outArgs, QString* err);

signals:
    void progress(qint64 received, qint64 total);
    void failed(const QString& reason);
    // Verified: ready to hand off to the helper and quit.
    void swapAndRestart(const QString& helperPath, const QStringList& args);

private:
    QUrl m_appImageUrl;
    QUrl m_sha256Url;
    QByteArray m_sha256Asset;   // the .sha256 file contents, once fetched
    QString m_downloadedImage;  // temp path of the downloaded AppImage
};
