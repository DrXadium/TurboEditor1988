// SPDX-License-Identifier: GPL-3.0-or-later
// Task 8: AutosaveService contract test.
// Proves: (1) registering a modified doc + one timer tick writes a recovery
// file under the autosave dir; (2) pendingRecovery() surfaces it; (3) accepting
// removes it (work was recovered/merged) and discarding removes it (work thrown
// away) — both clear the pending list.
#include <QtTest>
#include <QTemporaryDir>
#include <QUrl>
#include <QFile>
#include <QDir>
#include <QSignalSpy>
#include "src/autosave/AutosaveService.h"

class TstAutosave : public QObject
{
    Q_OBJECT
private slots:
    void tick_writes_recovery_file_and_lists_it() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        AutosaveService svc;
        svc.setAutosaveDir(dir.path());

        // A modified buffer whose original lives somewhere readable.
        const QUrl orig = QUrl::fromLocalFile("/etc/hostname");
        svc.registerDocument(orig, QStringLiteral("buffer contents v1"));
        svc.markModified(orig);

        QSignalSpy tick(&svc, &AutosaveService::recoveryAvailable);
        svc.runOneTick();          // deterministic single tick (no waiting)

        QVERIFY2(!svc.pendingRecovery().isEmpty(),
                 "pendingRecovery empty after a tick with a modified doc");
        QCOMPARE(tick.count(), 1);

        // The recovery file actually exists on disk under the autosave dir.
        const auto pending = svc.pendingRecovery();
        QVERIFY(!pending.isEmpty());
        QVERIFY2(QFile::exists(pending.first().autosavePath),
                 qPrintable("missing file: " + pending.first().autosavePath));
        QCOMPARE(pending.first().originalUrl, orig);
        QVERIFY(pending.first().id > 0);
    }

    void discardRecovery_removes_pending() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        AutosaveService svc;
        svc.setAutosaveDir(dir.path());
        const QUrl orig = QUrl::fromLocalFile("/etc/hostname");
        svc.registerDocument(orig, QStringLiteral("data"));
        svc.markModified(orig);
        svc.runOneTick();
        QVERIFY(!svc.pendingRecovery().isEmpty());

        const QString path = svc.pendingRecovery().first().autosavePath;
        svc.discardRecovery(svc.pendingRecovery().first().id);
        QVERIFY2(svc.pendingRecovery().isEmpty(), "discard left pending entries");
        QVERIFY2(!QFile::exists(path), "discard left the file on disk");
    }

    void acceptRecovery_removes_pending() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        AutosaveService svc;
        svc.setAutosaveDir(dir.path());
        const QUrl orig = QUrl::fromLocalFile("/etc/hostname");
        svc.registerDocument(orig, QStringLiteral("data"));
        svc.markModified(orig);
        svc.runOneTick();
        QVERIFY(!svc.pendingRecovery().isEmpty());
        const QString path = svc.pendingRecovery().first().autosavePath;
        svc.acceptRecovery(svc.pendingRecovery().first().id);
        QVERIFY(svc.pendingRecovery().isEmpty());
        QVERIFY2(!QFile::exists(path), "accept left the file on disk");
    }

    void cold_start_scan_detects_leftover_recovery_files() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        // Simulate a previous crashed run: a recovery file already on disk.
        {
            QDir().mkpath(dir.path());
            QFile f(dir.path() + "/42.txt");
            QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
            f.write("leftover from crash");
            f.close();
        }
        // Cold start pointed at that store -> the scan must surface the file.
        AutosaveService svc(dir.path());
        QVERIFY2(!svc.pendingRecovery().isEmpty(),
                 "cold-start scan missed a pre-existing recovery file");
        QCOMPARE(svc.pendingRecovery().first().id, 42);
        QVERIFY(QFile::exists(svc.pendingRecovery().first().autosavePath));
    }

    void unmodified_doc_not_snapshotted() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        AutosaveService svc;
        svc.setAutosaveDir(dir.path());
        const QUrl orig = QUrl::fromLocalFile("/etc/hostname");
        svc.registerDocument(orig, QStringLiteral("data"));
        // NOT marked modified -> one tick should snapshot nothing.
        svc.runOneTick();
        QVERIFY(svc.pendingRecovery().isEmpty());
    }
};

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    TstAutosave tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "tst_autosave.moc"