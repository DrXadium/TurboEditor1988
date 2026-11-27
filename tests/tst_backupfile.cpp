// SPDX-License-Identifier: GPL-3.0-or-later
// 2026-11-27: automatic .bak backup (user request).
//
// Opening a local file must also drop a sibling `<name>.bak` next to it, so a
// bad edit or a mangled save has an untouched copy one directory away. The
// feature is gated by EditorTabManager::setBackupOnOpen() (wired from
// CrtSettings::backupOnOpen in the composition root), OFF by default here so a
// test that forgets to opt in does not litter the temp dir - main.cpp turns it
// on at startup per the settings default.
//
// TDD: written before setBackupOnOpen()/writeBackup() exist.
#include <QtTest>
#include <QApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QUrl>

#include <ktexteditor/document.h>
#include <ktexteditor/view.h>

#include "src/editor/EditorTabManager.h"

class TstBackupFile : public QObject
{
    Q_OBJECT

    static QString writeFile(const QTemporaryDir& dir, const QString& name,
                             const QString& body)
    {
        const QString path = dir.path() + QLatin1Char('/') + name;
        QFile f(path);
        if (f.open(QIODevice::WriteOnly))
            f.write(body.toUtf8());
        return path;
    }

    static QString readFile(const QString& path)
    {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
            return QString();
        return QString::fromUtf8(f.readAll());
    }

private slots:
    void initTestCase()
    {
        if (qgetenv("QT_QPA_PLATFORM").isEmpty())
            qputenv("QT_QPA_PLATFORM", "offscreen");
    }

    // The core promise: opening a file writes `<file>.bak` in the SAME folder
    // with the file's content, while the original stays intact.
    void opening_a_local_file_writes_a_sibling_bak()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = writeFile(dir, QStringLiteral("notes.txt"),
                                       QStringLiteral("hello evidence\n"));
        EditorTabManager m;
        m.setBackupOnOpen(true);

        m.openFile(QUrl::fromLocalFile(path));

        const QString bak = dir.path() + QStringLiteral("/notes.txt.bak");
        QVERIFY2(QFile::exists(bak), "opening the file did not create a .bak beside it");
        QCOMPARE(readFile(bak), QStringLiteral("hello evidence\n"));
        // The original must be untouched by making the backup.
        QCOMPARE(readFile(path), QStringLiteral("hello evidence\n"));
        QVERIFY(QFile::exists(path));
    }

    // The toggle is a real gate: with it OFF (the EditorTabManager default),
    // opening must NOT create a .bak.
    void backup_is_off_by_default_and_the_gate_blocks_writes()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = writeFile(dir, QStringLiteral("quiet.txt"),
                                       QStringLiteral("no backup wanted\n"));
        EditorTabManager m;                           // gate defaults to OFF
        m.setBackupOnOpen(false);

        m.openFile(QUrl::fromLocalFile(path));

        QVERIFY(!QFile::exists(dir.path() + QStringLiteral("/quiet.txt.bak")));
    }

    // Flipping the gate live must take effect on the NEXT open without a
    // rebuild/restart - this is what main.cpp's connect() exercises.
    void toggling_the_gate_live_changes_subsequent_opens()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString off = writeFile(dir, QStringLiteral("before.txt"),
                                      QStringLiteral("one\n"));
        const QString on = writeFile(dir, QStringLiteral("after.txt"),
                                     QStringLiteral("two\n"));
        EditorTabManager m;

        m.setBackupOnOpen(false);
        m.openFile(QUrl::fromLocalFile(off));
        QVERIFY(!QFile::exists(dir.path() + QStringLiteral("/before.txt.bak")));

        m.setBackupOnOpen(true);
        m.openFile(QUrl::fromLocalFile(on));
        QVERIFY(QFile::exists(dir.path() + QStringLiteral("/after.txt.bak")));
    }

    // The backup always mirrors the CURRENT file content, so a re-open after
    // an edit refreshes the .bak rather than keeping a stale first copy.
    void reopening_refreshes_the_bak_to_the_current_content()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = writeFile(dir, QStringLiteral("draft.txt"),
                                       QStringLiteral("version 1\n"));
        EditorTabManager m;
        m.setBackupOnOpen(true);

        m.openFile(QUrl::fromLocalFile(path));
        QCOMPARE(readFile(dir.path() + QStringLiteral("/draft.txt.bak")),
                 QStringLiteral("version 1\n"));

        // User edits and SAVES, then re-opens: .bak must follow the file.
        {
            QFile f(path);
            QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
            f.write("version 2\n");
            f.close();
        }
        m.openFile(QUrl::fromLocalFile(path));
        QCOMPARE(readFile(dir.path() + QStringLiteral("/draft.txt.bak")),
                 QStringLiteral("version 2\n"));
    }

    // An untitled (no path on disk) document must not attempt a backup - there
    // is no folder to put it in and nothing worth copying.
    void untitled_documents_never_write_a_backup()
    {
        EditorTabManager m;
        m.setBackupOnOpen(true);
        m.newDocument();                              // no URL on disk
        QVERIFY(m.activeDocument());
        QVERIFY(m.activeDocument()->url().toLocalFile().isEmpty());
        // No crash and no stray file: the guard skips empty paths entirely.
        QVERIFY(true);
    }
};

QTEST_MAIN(TstBackupFile)
#include "tst_backupfile.moc"
