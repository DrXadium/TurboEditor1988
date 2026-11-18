// SPDX-License-Identifier: GPL-3.0-or-later
// Task 7: SessionManager contract test.
// Proves: (1) a session with 2 files + per-file cursor + active index survives a
// save/restore round-trip through a FRESH manager instance (i.e. real disk IO);
// (2) lastSession() reflects the saved session; (3) restoring a missing session
// fails cleanly instead of pretending success.
#include <QtTest>
#include <QTemporaryDir>
#include <QUrl>
#include "src/session/SessionManager.h"
#include "src/AppIdentity.h"

class TstSession : public QObject
{
    Q_OBJECT
private slots:
    // Rename guard: the app is now TURBO Kate, but saved sessions must stay
    // where they already are (~/.config/CRT-Kate/sessions). If the default dir
    // follows the new display name, every existing session is orphaned.
    void default_sessions_dir_stays_under_legacy_storage() {
        SessionManager m;
        QVERIFY2(m.sessionsDir().endsWith(QStringLiteral("/CRT-Kate/sessions")),
                 qPrintable(QStringLiteral("sessions dir moved to '%1'").arg(m.sessionsDir())));
        QVERIFY2(m.sessionsDir().contains(AppIdentity::storageDirName()),
                 qPrintable(QStringLiteral("sessions dir '%1' does not use the storage id '%2'")
                                .arg(m.sessionsDir(), AppIdentity::storageDirName())));
    }

    void save_restore_roundtrip_two_files_cursor() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        SessionManager saver;
        saver.setSessionsDir(dir.path());
        saver.setEntries({
            { QUrl::fromLocalFile("/tmp/proj/main.cpp"), 12, 5, 0 },
            { QUrl::fromLocalFile("/tmp/proj/util.h"), 3, 9, 1 },
        });
        saver.saveSession("proj1");

        // Fresh instance: everything must come back off disk.
        SessionManager loader;
        loader.setSessionsDir(dir.path());
        QVERIFY2(loader.restoreSession("proj1"), "restoreSession failed for saved session");
        const auto entries = loader.entries();
        QCOMPARE(entries.size(), 2);
        QCOMPARE(entries[0].fileUrl, QUrl::fromLocalFile("/tmp/proj/main.cpp"));
        QCOMPARE(entries[0].line, 12);
        QCOMPARE(entries[0].column, 5);
        QCOMPARE(entries[0].activeIndex, 0);
        QCOMPARE(entries[1].fileUrl, QUrl::fromLocalFile("/tmp/proj/util.h"));
        QCOMPARE(entries[1].line, 3);
        QCOMPARE(entries[1].column, 9);
        QCOMPARE(entries[1].activeIndex, 1);
        QCOMPARE(loader.lastSession(), QStringLiteral("proj1"));
    }

    void restore_missing_session_returns_false() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        SessionManager m;
        m.setSessionsDir(dir.path());
        QVERIFY(!m.restoreSession("does-not-exist"));
        QVERIFY(m.entries().isEmpty());
    }

    void sessions_appear_in_directory_listing() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        SessionManager m;
        m.setSessionsDir(dir.path());
        m.setEntries({ { QUrl::fromLocalFile("/tmp/a.txt"), 1, 1, 0 } });
        m.saveSession("alpha");
        m.saveSession("beta");
        QVERIFY(m.sessionNames().contains("alpha"));
        QVERIFY(m.sessionNames().contains("beta"));
        QCOMPARE(m.sessionNames().size(), 2);
    }

    // Autosave round (approach A): timestamped snapshots named
    // auto-YYYYMMDD-HHMMSS.json, written on demand with an injectable clock so
    // the filename is deterministic. The snapshot must be a real session -
    // restorable through the normal restore path and visible in sessionNames().
    void take_snapshot_writes_timestamped_auto_file_and_is_restorable() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        SessionManager m;
        m.setSessionsDir(dir.path());
        m.setEntries({
            { QUrl::fromLocalFile("/tmp/proj/main.cpp"), 7, 2, 0 },
            { QUrl::fromLocalFile("/tmp/proj/util.h"),   3, 9, 1 },
        });

        // 2026-11-17 09:05:07 local -> auto-20261117-090507.json
        const QDateTime fixed(QDate(2026, 11, 17), QTime(9, 5, 7),
                              QTimeZone::UTC);
        const QString name = m.takeSnapshot(fixed);
        QCOMPARE(name, QStringLiteral("auto-20261117-090507"));
        QVERIFY2(QFile::exists(dir.filePath(name + QStringLiteral(".json"))),
                 "snapshot file missing on disk");

        // A fresh instance restores it like any named session.
        SessionManager fresh;
        fresh.setSessionsDir(dir.path());
        QVERIFY2(fresh.restoreSession(name), "snapshot did not restore");
        const auto entries = fresh.entries();
        QCOMPARE(entries.size(), 2);
        QCOMPARE(entries[0].fileUrl, QUrl::fromLocalFile("/tmp/proj/main.cpp"));
        QCOMPARE(entries[0].line, 7);
        QCOMPARE(entries[1].activeIndex, 1);

        // Listed alongside named sessions (so the UI can show history).
        QVERIFY(fresh.sessionNames().contains(name));
    }

    // Default (no clock argument) uses the current time and still follows the
    // same naming contract.
    void take_snapshot_defaults_to_a_valid_auto_name() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        SessionManager m;
        m.setSessionsDir(dir.path());
        m.setEntries({ { QUrl::fromLocalFile("/tmp/a.txt"), 1, 1, 0 } });

        const QString name = m.takeSnapshot();
        QVERIFY2(QRegularExpression(
                     QStringLiteral("^auto-\\d{8}-\\d{6}$")).match(name).hasMatch(),
                 qPrintable(QStringLiteral("bad snapshot name '%1'").arg(name)));
        QVERIFY(QFile::exists(dir.filePath(name + QStringLiteral(".json"))));
    }

    // Autosave round: only the newest 10 auto- snapshots survive; older ones
    // are pruned and NAMED sessions are never touched.
    void auto_snapshots_rotate_keeping_the_newest_10() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        SessionManager m;
        m.setSessionsDir(dir.path());
        m.setEntries({ { QUrl::fromLocalFile("/tmp/a.txt"), 1, 1, 0 } });

        // A named session that must survive the rotation.
        m.saveSession("important");
        QVERIFY(QFile::exists(dir.filePath("important.json")));

        // 14 snapshots, one per minute, in order.
        for (int i = 0; i < 14; ++i) {
            const QDateTime at(QDate(2026, 11, 17), QTime(9, 0, 0)
                                           .addSecs(i * 60), QTimeZone::UTC);
            m.takeSnapshot(at);
        }

        const auto autos = QStringList(m.sessionNames())
                               .filter(QRegularExpression(QStringLiteral("^auto-")));
        QCOMPARE(autos.size(), 10);
        QVERIFY2(m.sessionNames().contains("important"),
                 "rotation deleted a named session");

        // Oldest are gone, newest 12/13 survive: 09:00..09:13 written, so
        // 09:00..09:03 must be pruned and 09:13 must remain.
        QVERIFY(!QFile::exists(dir.filePath("auto-20261117-090000.json")));
        QVERIFY(!QFile::exists(dir.filePath("auto-20261117-090300.json")));
        QVERIFY(QFile::exists(dir.filePath("auto-20261117-091200.json")));
        QVERIFY(QFile::exists(dir.filePath("auto-20261117-091300.json")));
    }

    // A snapshot written when the store already holds MORE than the retention
    // limit prunes immediately, so the count is never allowed to drift.
    void rotation_holds_even_when_starting_over_the_limit() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        SessionManager m;
        m.setSessionsDir(dir.path());
        m.setEntries({ { QUrl::fromLocalFile("/tmp/a.txt"), 1, 1, 0 } });

        // 13 pre-existing auto- snapshots on disk (as a previous run left them).
        for (int i = 0; i < 13; ++i) {
            const QString n = QStringLiteral("auto-2026010%1-000000").arg(i);
            QFile f(dir.filePath(n + QStringLiteral(".json")));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("{}");
            f.close();
        }
        const QString name = m.takeSnapshot();
        const auto autos = QStringList(m.sessionNames())
                               .filter(QRegularExpression(QStringLiteral("^auto-")));
        QCOMPARE(autos.size(), 10);
        QVERIFY(autos.contains(name));   // newest survives
    }
};

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    TstSession tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "tst_session.moc"