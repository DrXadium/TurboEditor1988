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
};

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    TstSession tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "tst_session.moc"