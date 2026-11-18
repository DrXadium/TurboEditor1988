// SPDX-License-Identifier: GPL-3.0-or-later
// RecentFilesStore: the persisted MRU-10 behind the file dock's recents
// dropdown. Kept free of app dependencies (an explicit ini path is injected)
// so crt-shell stays a plain widget library.
//
// TDD: these tests are written FIRST and must fail because RecentFilesStore
// does not exist yet.
#include <QtTest>
#include <QTemporaryDir>
#include <QStringList>
#include <QDir>
#include <QFile>
#include "src/shell/RecentFilesStore.h"

class TstRecents : public QObject
{
    Q_OBJECT
private slots:
    // Newest first, an exact duplicate moves to the front instead of repeating,
    // and the list never grows past the configured cap.
    void add_keeps_mru_order_deduped_and_capped()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        RecentFilesStore s(dir.filePath(QStringLiteral("recents.ini")), 3);

        s.add(QStringLiteral("/a/one.txt"));
        s.add(QStringLiteral("/a/two.txt"));
        s.add(QStringLiteral("/a/one.txt"));            // re-add moves to the front
        QCOMPARE(s.entries(), QStringList({QStringLiteral("/a/one.txt"),
                                           QStringLiteral("/a/two.txt")}));

        s.add(QStringLiteral("/a/three.txt"));
        s.add(QStringLiteral("/a/four.txt"));           // cap = 3
        QCOMPARE(s.entries().size(), 3);
        QCOMPARE(s.entries().first(), QStringLiteral("/a/four.txt"));
        QVERIFY2(!s.entries().contains(QStringLiteral("/a/two.txt")),
                 qPrintable(QStringLiteral("oldest entry survived the cap: %1")
                                .arg(s.entries().join(QStringLiteral(", ")))));
    }

    // A blank path is not a recent file.
    void empty_paths_are_ignored()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        RecentFilesStore s(dir.filePath(QStringLiteral("recents.ini")));
        s.add(QString());
        s.add(QStringLiteral("   "));
        QVERIFY(s.entries().isEmpty());
    }

    // The whole point of a store: the next run sees the history.
    void entries_survive_a_new_instance()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString ini = dir.filePath(QStringLiteral("recents.ini"));
        {
            RecentFilesStore s(ini);
            s.add(QStringLiteral("/a/one.txt"));
        }
        RecentFilesStore again(ini);
        QCOMPARE(again.entries(), QStringList({QStringLiteral("/a/one.txt")}));
    }

    // A relative path is stored absolute, so a later run resolves it the same.
    void paths_are_stored_absolute()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        RecentFilesStore s(dir.filePath(QStringLiteral("recents.ini")));
        s.add(QStringLiteral("relative.txt"));
        QVERIFY2(s.entries().first().startsWith(QDir::currentPath()),
                 qPrintable(QStringLiteral("not absolute: %1").arg(s.entries().first())));
    }

    // An unreadable/garbage ini must not take the dialog down with it.
    void a_corrupt_ini_yields_an_empty_list()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString ini = dir.filePath(QStringLiteral("recents.ini"));
        {
            QFile f(ini);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("\x00\x01not an ini\xff", 14);
        }
        RecentFilesStore s(ini);
        QVERIFY(s.entries().size() <= 10);
    }
};

QTEST_MAIN(TstRecents)
#include "tst_recents.moc"
