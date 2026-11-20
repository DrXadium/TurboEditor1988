// SPDX-License-Identifier: GPL-3.0-or-later
// 2026-11-20: FileChangeTracker - the pure decision layer behind the external
// change prompt. Kate's own modifiedOnDisk signal NEVER fires in this build
// (the library contains no file watcher at all; a live probe rewrote an open
// file and the signal stayed silent), so the app compares file signatures
// itself. Keeping the comparison free of widgets lets every rule be pinned
// without a modal exec.
//
// TDD: written before FileChangeTracker exists.
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>

#include "src/editor/FileChangeTracker.h"

class TstFileChange : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase() {
        if (qgetenv("QT_QPA_PLATFORM").isEmpty())
            qputenv("QT_QPA_PLATFORM", "offscreen");
    }

    void signature_of_a_real_file_reports_exists_size_and_mtime() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("a.txt"));
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("hello");
        f.close();

        const FileSignature sig = FileSignature::fromPath(path);
        QVERIFY(sig.exists);
        QCOMPARE(sig.size, qint64(5));
        QVERIFY(sig.mtimeMs > 0);
    }

    void signature_of_a_missing_file_reports_not_exists() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const FileSignature sig = FileSignature::fromPath(dir.filePath(QStringLiteral("gone.txt")));
        QVERIFY(!sig.exists);
    }

    void unchanged_signature_is_none() {
        FileChangeTracker t;
        const QString path = QStringLiteral("/tmp/x");
        const FileSignature sig{true, 1000, 10};
        t.setBaseline(path, sig);
        QCOMPARE(t.takeAction(path, sig), FileChange::None);
        QCOMPARE(t.takeAction(path, sig), FileChange::None);
    }

    void changed_mtime_is_modified() {
        FileChangeTracker t;
        const QString path = QStringLiteral("/tmp/x");
        t.setBaseline(path, FileSignature{true, 1000, 10});
        QCOMPARE(t.takeAction(path, FileSignature{true, 2000, 10}), FileChange::Modified);
    }

    void changed_size_is_modified() {
        FileChangeTracker t;
        const QString path = QStringLiteral("/tmp/x");
        t.setBaseline(path, FileSignature{true, 1000, 10});
        QCOMPARE(t.takeAction(path, FileSignature{true, 1000, 11}), FileChange::Modified);
    }

    void deletion_prompts_once_then_goes_quiet() {
        FileChangeTracker t;
        const QString path = QStringLiteral("/tmp/x");
        t.setBaseline(path, FileSignature{true, 1000, 10});
        const FileSignature gone{false, 0, -1};
        QCOMPARE(t.takeAction(path, gone), FileChange::Deleted);
        QCOMPARE(t.takeAction(path, gone), FileChange::None);
    }

    void a_file_that_reappears_is_modified() {
        FileChangeTracker t;
        const QString path = QStringLiteral("/tmp/x");
        t.setBaseline(path, FileSignature{true, 1000, 10});
        QCOMPARE(t.takeAction(path, FileSignature{false, 0, -1}), FileChange::Deleted);
        QCOMPARE(t.takeAction(path, FileSignature{true, 3000, 20}), FileChange::Modified);
    }

    void rebaselining_after_our_own_write_is_silent() {
        FileChangeTracker t;
        const QString path = QStringLiteral("/tmp/x");
        t.setBaseline(path, FileSignature{true, 1000, 10});
        // Our own save: the file now carries the new signature.
        const FileSignature afterSave{true, 2000, 12};
        t.setBaseline(path, afterSave);
        QCOMPARE(t.takeAction(path, afterSave), FileChange::None);
    }

    void unknown_path_is_none() {
        FileChangeTracker t;
        QCOMPARE(t.takeAction(QStringLiteral("/tmp/never-seen"), FileSignature{true, 1, 1}),
                 FileChange::None);
        QVERIFY(!t.isTracking(QStringLiteral("/tmp/never-seen")));
    }

    void forget_stops_tracking() {
        FileChangeTracker t;
        const QString path = QStringLiteral("/tmp/x");
        t.setBaseline(path, FileSignature{true, 1000, 10});
        QVERIFY(t.isTracking(path));
        t.forget(path);
        QVERIFY(!t.isTracking(path));
        QCOMPARE(t.takeAction(path, FileSignature{true, 9000, 99}), FileChange::None);
    }
};

QTEST_MAIN(TstFileChange)
#include "tst_filechange.moc"
