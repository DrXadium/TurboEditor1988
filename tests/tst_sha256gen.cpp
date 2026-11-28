// SPDX-License-Identifier: GPL-3.0-or-later
// The release pipeline must publish a .sha256 that the updater accepts. This
// test shells out to scripts/gen_sha256.sh and then feeds the generated file to
// Checksum::verifyFile — the exact parser UpdateRunner uses — so a format drift
// between "what we publish" and "what we accept" fails here, not in the field.
#include <QtTest>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include "src/update/Checksum.h"

namespace {
const char kFixtureName[] = "TurboEditor1988-x86_64.AppImage";

QString genScript()
{
    const QStringList candidates{
        QStringLiteral("../scripts/gen_sha256.sh"),
        QStringLiteral("scripts/gen_sha256.sh"),
        QStringLiteral("/var/home/dr_xadium/projects/CRT_Kate/scripts/gen_sha256.sh"),
    };
    for (const QString& c : candidates)
        if (QFile::exists(c))
            return c;
    return {};
}

struct Run {
    int code = -1;
    QString err;
};

Run runGen(const QString& path)
{
    QProcess p;
    p.start(QStringLiteral("/bin/sh"), {genScript(), path});
    p.waitForFinished(10000);
    Run r;
    r.code = p.exitCode();
    r.err = QString::fromLocal8Bit(p.readAllStandardError());
    return r;
}

QByteArray readAll(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return f.readAll();
}

void writeFixture(const QString& path, const QByteArray& bytes)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(bytes);
    f.close();
}
} // namespace

class TstSha256gen : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QVERIFY2(!genScript().isEmpty(), "scripts/gen_sha256.sh not found");
    }

    // The published digest must be "<hex>  <name>" and must parse as valid
    // through the updater's own verifier.
    void generated_checksum_is_accepted_by_the_updater()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString image = dir.filePath(QLatin1String(kFixtureName));
        writeFixture(image, QByteArrayLiteral("pretend this is a 150MB AppImage"));

        const Run r = runGen(image);
        QVERIFY2(r.code == 0, qPrintable(QStringLiteral("generator failed: %1").arg(r.err)));

        const QString sumFile = image + QStringLiteral(".sha256");
        QVERIFY2(QFile::exists(sumFile), "no .sha256 next to the AppImage");

        const QByteArray body = readAll(sumFile);
        QVERIFY2(!body.isEmpty(), ".sha256 is empty");

        // Format: lowercase hex, two spaces, then the bare file name.
        const QString text = QString::fromLatin1(body).trimmed();
        const QString expected = QString::fromLatin1(Checksum::sha256File(image))
                               + QStringLiteral("  ") + QLatin1String(kFixtureName);
        QCOMPARE(text, expected);

        // The real contract: the updater's verifier accepts what we published.
        QVERIFY2(Checksum::verifyFile(image, text),
                 "the generated .sha256 is rejected by Checksum::verifyFile");
    }

    // Re-running the build must overwrite, never append a second line.
    void regenerating_overwrites_rather_than_appends()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString image = dir.filePath(QLatin1String(kFixtureName));
        writeFixture(image, QByteArrayLiteral("first"));

        QCOMPARE(runGen(image).code, 0);
        const QByteArray first = readAll(image + QStringLiteral(".sha256"));

        QCOMPARE(runGen(image).code, 0);
        const QByteArray second = readAll(image + QStringLiteral(".sha256"));

        QCOMPARE(second, first);
        QCOMPARE(second.count('\n'), 1);      // exactly one line, not two
    }

    // A missing input is a build error, not a silently empty checksum file.
    void missing_input_fails_and_writes_nothing()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString image = dir.filePath(QLatin1String(kFixtureName));

        const Run r = runGen(image);
        QVERIFY2(r.code != 0, "generator succeeded on a missing file");
        QVERIFY2(!QFile::exists(image + QStringLiteral(".sha256")),
                 "generator wrote a .sha256 for a file that does not exist");
    }
};

QTEST_APPLESS_MAIN(TstSha256gen)
#include "tst_sha256gen.moc"
