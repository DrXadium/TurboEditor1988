// SPDX-License-Identifier: GPL-3.0-or-later
// 2026-11-20: external-change wiring. Kate has no file watcher at all (its
// library carries no QFileSystemWatcher and modifiedOnDisk never fires), so
// EditorTabManager owns one. These tests drive REAL files through a REAL
// QFileSystemWatcher - the interesting failures (our own save prompting, a
// deleted path silently dropping out of the watch, reload leaving an undo step)
// only show up with a real file.
//
// TDD: written before the watcher wiring exists.
#include <QtTest>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QUrl>

#include <ktexteditor/document.h>
#include <ktexteditor/view.h>

#include "src/editor/EditorTabManager.h"
#include "src/editor/FileChangeTracker.h"
#include "src/shell/TvModal.h"

namespace {
void writeFile(const QString& path, const QString& text)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(text.toUtf8());
    f.close();
}
}

class TstReload : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase() {
        if (qgetenv("QT_QPA_PLATFORM").isEmpty())
            qputenv("QT_QPA_PLATFORM", "offscreen");
    }

    void external_write_prompts_once() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("doc.txt"));
        writeFile(path, QStringLiteral("one\n"));

        EditorTabManager m;
        int prompts = 0;
        FileChange seen = FileChange::None;
        m.setExternalChangeHandler([&](FileChange kind, const QString&) {
            ++prompts;
            seen = kind;
            return int(TvModal::Reject);
        });
        m.openFile(QUrl::fromLocalFile(path));
        QVERIFY(m.isWatchingPath(path));

        writeFile(path, QStringLiteral("one\ntwo\n"));

        QTRY_COMPARE_WITH_TIMEOUT(prompts, 1, 5000);
        QCOMPARE(seen, FileChange::Modified);
    }

    void two_external_writes_prompt_twice() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("doc.txt"));
        writeFile(path, QStringLiteral("a\n"));

        EditorTabManager m;
        int prompts = 0;
        m.setExternalChangeHandler([&](FileChange, const QString&) {
            ++prompts;
            return int(TvModal::Reject);
        });
        m.openFile(QUrl::fromLocalFile(path));

        writeFile(path, QStringLiteral("a\nb\n"));
        QTRY_COMPARE_WITH_TIMEOUT(prompts, 1, 5000);

        writeFile(path, QStringLiteral("a\nb\nc\n"));
        QTRY_COMPARE_WITH_TIMEOUT(prompts, 2, 5000);
    }

    void our_own_save_does_not_prompt() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("doc.txt"));
        writeFile(path, QStringLiteral("one\n"));

        EditorTabManager m;
        int prompts = 0;
        m.setExternalChangeHandler([&](FileChange, const QString&) {
            ++prompts;
            return int(TvModal::Reject);
        });
        m.openFile(QUrl::fromLocalFile(path));

        m.activeDocument()->setText(QStringLiteral("one\nsaved by us\n"));
        QVERIFY(m.saveActiveAs(QUrl::fromLocalFile(path)));
        QTest::qWait(400);

        QCOMPARE(prompts, 0);
    }

    void reload_replaces_text_and_leaves_no_undo_step() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("doc.txt"));
        writeFile(path, QStringLiteral("old\n"));

        EditorTabManager m;
        m.setExternalChangeHandler([](FileChange, const QString&) {
            return int(TvModal::Reject);
        });
        m.openFile(QUrl::fromLocalFile(path));
        KTextEditor::Document* doc = m.activeDocument();
        QVERIFY(doc);

        writeFile(path, QStringLiteral("new\ncontent\n"));
        m.reloadDocument(doc);

        QCOMPARE(doc->text(), QStringLiteral("new\ncontent\n"));
        QVERIFY2(!m.canUndo(), "reload must not leave the discarded buffer undoable");
    }

    void deleted_file_keep_keeps_the_buffer_and_save_recreates_it() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("doc.txt"));
        writeFile(path, QStringLiteral("keep me\n"));

        EditorTabManager m;
        int prompts = 0;
        FileChange seen = FileChange::None;
        m.setExternalChangeHandler([&](FileChange kind, const QString&) {
            ++prompts;
            seen = kind;
            return int(TvModal::Accept);      // Keep
        });
        m.openFile(QUrl::fromLocalFile(path));
        const int tabsBefore = m.tabCount();

        QVERIFY(QFile::remove(path));
        QTRY_COMPARE_WITH_TIMEOUT(prompts, 1, 5000);
        QCOMPARE(seen, FileChange::Deleted);

        QCOMPARE(m.tabCount(), tabsBefore);
        QVERIFY(m.activeDocument());
        QCOMPARE(m.activeDocument()->text(), QStringLiteral("keep me\n"));

        // The buffer is still saveable, and saving recreates the file and
        // re-arms the watch (Qt dropped the deleted path).
        QVERIFY(m.saveActiveAs(QUrl::fromLocalFile(path)));
        QVERIFY(QFile::exists(path));
        QVERIFY(m.isWatchingPath(path));
    }

    void deleted_file_close_removes_the_tab() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("doc.txt"));
        writeFile(path, QStringLiteral("bye\n"));

        EditorTabManager m;
        int prompts = 0;
        m.setExternalChangeHandler([&](FileChange, const QString&) {
            ++prompts;
            return int(TvModal::Reject);      // Close Tab
        });
        m.openFile(QUrl::fromLocalFile(path));
        const int tabsBefore = m.tabCount();
        QCOMPARE(tabsBefore, 2);              // constructor tab + opened file

        QVERIFY(QFile::remove(path));
        QTRY_COMPARE_WITH_TIMEOUT(prompts, 1, 5000);
        QCOMPARE(m.tabCount(), 1);
    }

    void untitled_buffer_is_not_watched() {
        EditorTabManager m;
        QVERIFY(!m.isWatchingPath(QString()));
        m.newDocument();
        QVERIFY(!m.isWatchingPath(QString()));
    }
};

QTEST_MAIN(TstReload)
#include "tst_reload.moc"
