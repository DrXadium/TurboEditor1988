// SPDX-License-Identifier: GPL-3.0-or-later
// Task 4 (auto-update): SHA-256 verification of a downloaded file against the
// hex digest from the release's .sha256 asset ("<hex>  <name>").
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include "src/update/Checksum.h"

class TstChecksum : public QObject
{
    Q_OBJECT
private slots:
    void sha256_of_known_bytes()
    {
        // sha256("abc") is a fixed, well-known value.
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString p = dir.filePath(QStringLiteral("abc.txt"));
        QFile f(p);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("abc");
        f.close();
        QCOMPARE(Checksum::sha256File(p).toLower(),
                 QByteArray("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
    }

    void verify_file_matches_digest()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString p = dir.filePath(QStringLiteral("x.bin"));
        QFile f(p);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("abc");
        f.close();
        QVERIFY(Checksum::verifyFile(
            p, QStringLiteral("BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD")));
    }

    void verify_file_rejects_mismatch()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString p = dir.filePath(QStringLiteral("x.bin"));
        QFile f(p);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("abc");
        f.close();
        QVERIFY(!Checksum::verifyFile(p, QStringLiteral("deadbeef")));
    }

    void verify_file_rejects_missing_file()
    {
        QVERIFY(!Checksum::verifyFile(QStringLiteral("/nonexistent/nowhere"),
                                      QStringLiteral("deadbeef")));
    }
};

QTEST_APPLESS_MAIN(TstChecksum)
#include "tst_checksum.moc"
