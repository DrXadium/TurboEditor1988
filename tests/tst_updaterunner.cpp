// SPDX-License-Identifier: GPL-3.0-or-later
// Task 5 (auto-update): UpdateRunner verifies a payload against a .sha256
// digest and stages the swap helper. It refuses on a checksum mismatch (never
// emitting readyToSwap) and only reports a helper when running from an
// AppImage (APPIMAGE env set) — a non-AppImage build has nothing to swap.
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QSignalSpy>
#include "src/update/UpdateRunner.h"
#include "src/update/Checksum.h"

namespace {
// Write a file and return its sha256 hex (using the real Checksum, which is
// itself under test in tst_checksum).
QString writeFileReturnSha(QTemporaryDir& dir, const QString& name, const QByteArray& bytes)
{
    const QString p = dir.filePath(name);
    QFile f(p);
    if (!f.open(QIODevice::WriteOnly))
        return {};
    f.write(bytes);
    f.close();
    return QString::fromLatin1(Checksum::sha256File(p));
}
} // namespace

class TstUpdaterunner : public QObject
{
    Q_OBJECT
private slots:
    void refuses_when_checksum_mismatch()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString payload = dir.filePath(QStringLiteral("payload.bin"));
        const QString goodSha =
            writeFileReturnSha(dir, QStringLiteral("other.bin"), QByteArrayLiteral("abc"));
        {
            QFile f(payload);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("different-bytes");
            f.close();
        }
        QString err;
        QString helper;
        QStringList args;
        QVERIFY2(!UpdateRunner::prepareSwap(payload, goodSha, &helper, &args, &err),
                 qPrintable(QStringLiteral("mismatch was accepted: %1").arg(err)));
        QVERIFY(helper.isEmpty());
    }

    void prepareSwap_writes_executable_helper()
    {
        // prepareSwap only proceeds for an AppImage run; simulate one by
        // setting the env the runtime would set, then restore it.
        const QByteArray saved = qgetenv("APPIMAGE");
        qputenv("APPIMAGE", QByteArrayLiteral("/tmp/fake.AppImage"));
        auto restore = qScopeGuard([saved]() {
            if (saved.isEmpty())
                qunsetenv("APPIMAGE");
            else
                qputenv("APPIMAGE", saved);
        });

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString payload = dir.filePath(QStringLiteral("payload.bin"));
        const QString sha = writeFileReturnSha(dir, QStringLiteral("p.bin"),
                                               QByteArrayLiteral("abc"));
        {
            QFile f(payload);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("abc");
            f.close();
        }
        QString err, helper;
        QStringList args;
        QVERIFY2(UpdateRunner::prepareSwap(payload, sha, &helper, &args, &err),
                 qPrintable(err));
        QVERIFY(!helper.isEmpty());
        QVERIFY2(QFile::exists(helper), "helper script was not written");
        QVERIFY2(QFile::permissions(helper) & QFile::ExeUser, "helper is not executable");
        QCOMPARE(args.size(), 3);            // target, new image, expected sha
        QCOMPARE(args.at(0), QStringLiteral("/tmp/fake.AppImage"));
    }

    void isAppImageRun_false_without_env()
    {
        // This test binary is not running as an AppImage.
        QVERIFY(!qEnvironmentVariableIsSet("APPIMAGE"));
        QVERIFY(!UpdateRunner::isAppImageRun());
    }
};

QTEST_MAIN(TstUpdaterunner)
#include "tst_updaterunner.moc"
