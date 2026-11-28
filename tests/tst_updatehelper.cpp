// SPDX-License-Identifier: GPL-3.0-or-later
// Task 7 (auto-update): drive the REAL swap helper against scratch files.
// The helper must (a) swap when the checksum matches, (b) restore the .bak and
// fail when it doesn't, (c) fail cleanly when the target is missing. The final
// relaunch is suppressed in tests by passing /bin/true as the "exec" target is
// not possible (the script execs its own arg), so tests instead assert on file
// state + exit code and use a wrapper: we run the script with the new image,
// then check results. To avoid actually exec'ing a fake AppImage, the helper
// honors a CRT_KATE_UPDATE_NO_RELAUNCH=1 env to skip the final exec (test-only).
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QProcess>
#include "src/update/Checksum.h"

namespace {
QString helperScript()
{
    // Locate the repo copy (tests run from build/; source is one up).
    const QStringList candidates{
        QStringLiteral("../src/update/turbo-kate-update.sh"),
        QStringLiteral("src/update/turbo-kate-update.sh"),
        QStringLiteral("/var/home/dr_xadium/projects/CRT_Kate/src/update/turbo-kate-update.sh"),
    };
    for (const QString& c : candidates)
        if (QFile::exists(c))
            return c;
    return {};
}

struct Run { int code; QString out; };
Run runHelper(const QString& target, const QString& newimg, const QString& sha)
{
    QProcess p;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("CRT_KATE_UPDATE_NO_RELAUNCH"), QStringLiteral("1"));
    p.setProcessEnvironment(env);
    p.start(QStringLiteral("/bin/sh"), {helperScript(), target, newimg, sha});
    p.waitForFinished(8000);
    return {p.exitCode(), QString::fromLocal8Bit(p.readAllStandardError())};
}

void writeFile(const QString& path, const QByteArray& bytes)
{
    QFile f(path);
    f.open(QIODevice::WriteOnly | QIODevice::Truncate);
    f.write(bytes);
    f.close();
}
QByteArray readFile(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return f.readAll();
}
} // namespace

class TstUpdatehelper : public QObject
{
    Q_OBJECT
private slots:
    void swaps_when_checksum_ok()
    {
        QVERIFY2(!helperScript().isEmpty(), "helper script not found");
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString target = dir.filePath(QStringLiteral("app.AppImage"));
        const QString newimg = dir.filePath(QStringLiteral("new.AppImage"));
        writeFile(target, QByteArrayLiteral("OLD"));
        writeFile(newimg, QByteArrayLiteral("NEW"));
        const QString sha = QString::fromLatin1(Checksum::sha256File(newimg));

        const Run r = runHelper(target, newimg, sha);
        QVERIFY2(r.code == 0, qPrintable(QStringLiteral("helper failed: %1").arg(r.out)));
        QCOMPARE(readFile(target), QByteArrayLiteral("NEW"));
        QVERIFY2(!QFile::exists(target + QStringLiteral(".bak")),
                 ".bak left behind after a successful swap");
        QVERIFY2(!QFile::exists(newimg), "new image not consumed");
    }

    void restores_bak_when_checksum_bad()
    {
        QVERIFY2(!helperScript().isEmpty(), "helper script not found");
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString target = dir.filePath(QStringLiteral("app.AppImage"));
        const QString newimg = dir.filePath(QStringLiteral("new.AppImage"));
        writeFile(target, QByteArrayLiteral("OLD"));
        writeFile(newimg, QByteArrayLiteral("NEW"));
        const QString badSha = QStringLiteral("deadbeefdeadbeefdeadbeefdeadbeef"
                                              "deadbeefdeadbeefdeadbeefdeadbeef");

        const Run r = runHelper(target, newimg, badSha);
        QVERIFY2(r.code != 0, "helper accepted a bad checksum");
        QCOMPARE(readFile(target), QByteArrayLiteral("OLD"));   // untouched
        QVERIFY2(!QFile::exists(target + QStringLiteral(".bak")), ".bak should not exist");
    }

    void no_swap_when_target_missing()
    {
        QVERIFY2(!helperScript().isEmpty(), "helper script not found");
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString target = dir.filePath(QStringLiteral("nope.AppImage"));
        const QString newimg = dir.filePath(QStringLiteral("new.AppImage"));
        writeFile(newimg, QByteArrayLiteral("NEW"));
        const QString sha = QString::fromLatin1(Checksum::sha256File(newimg));

        const Run r = runHelper(target, newimg, sha);
        QVERIFY2(r.code != 0, "helper succeeded with a missing target");
        QVERIFY2(!QFile::exists(target), "helper created a target from nothing");
    }
};

QTEST_APPLESS_MAIN(TstUpdatehelper)
#include "tst_updatehelper.moc"
