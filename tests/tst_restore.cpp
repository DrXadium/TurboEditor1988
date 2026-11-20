// SPDX-License-Identifier: GPL-3.0-or-later
// 2026-11-20: launch-time session restore opened the file BEHIND an empty tab.
//
// Root cause, measured in the running app: EditorTabManager's constructor always
// creates one empty untitled tab, and openFile() APPENDS. A session whose
// activeIndex is 0 therefore selected the empty placeholder, not the restored
// file - the diagnostic read "tabs=2 url='' textLen=0" while the file sat in the
// other tab. Restore now replaces the placeholders instead of appending behind
// them, and never touches a buffer the user has actually typed into.
//
// TDD: written before replaceWithFiles() exists.
#include <QtTest>
#include <QApplication>
#include <QTemporaryDir>
#include <QFile>

#include <ktexteditor/document.h>
#include <ktexteditor/view.h>

#include "src/editor/EditorTabManager.h"

class TstRestore : public QObject
{
    Q_OBJECT

    static QString writeFile(const QTemporaryDir& dir, const QString& name, const QString& body)
    {
        const QString path = dir.path() + QLatin1Char('/') + name;
        QFile f(path);
        if (f.open(QIODevice::WriteOnly))
            f.write(body.toUtf8());
        return path;
    }

private slots:
    void initTestCase()
    {
        if (qgetenv("QT_QPA_PLATFORM").isEmpty())
            qputenv("QT_QPA_PLATFORM", "offscreen");
    }

    // The bug: the restored file must be the ACTIVE tab, and the empty
    // placeholder must be gone.
    void restoring_one_file_replaces_the_empty_placeholder()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = writeFile(dir, QStringLiteral("restored.txt"),
                                       QStringLiteral("alpha\nbeta\n"));
        EditorTabManager m;
        QCOMPARE(m.viewCount(), 1);          // the placeholder the ctor makes

        m.replaceWithFiles({QUrl::fromLocalFile(path)}, 0);

        QCOMPARE(m.viewCount(), 1);          // placeholder replaced, not appended
        QVERIFY(m.activeDocument());
        QCOMPARE(m.activeDocument()->url(), QUrl::fromLocalFile(path));
        QCOMPARE(m.activeDocument()->text(), QStringLiteral("alpha\nbeta\n"));
    }

    // The saved active index picks the tab the user was on.
    void the_saved_active_index_becomes_the_active_tab()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString a = writeFile(dir, QStringLiteral("a.txt"), QStringLiteral("AAA\n"));
        const QString b = writeFile(dir, QStringLiteral("b.txt"), QStringLiteral("BBB\n"));
        EditorTabManager m;

        m.replaceWithFiles({QUrl::fromLocalFile(a), QUrl::fromLocalFile(b)}, 1);

        QCOMPARE(m.viewCount(), 2);
        QCOMPARE(m.activeDocument()->url(), QUrl::fromLocalFile(b));
        QCOMPARE(m.activeDocument()->text(), QStringLiteral("BBB\n"));
    }

    // Safety: a buffer the user has typed into is NOT a placeholder. Restoring a
    // session must never silently discard unsaved work.
    void a_buffer_with_text_is_never_closed()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = writeFile(dir, QStringLiteral("restored.txt"),
                                       QStringLiteral("restored\n"));
        EditorTabManager m;
        m.activeDocument()->setText(QStringLiteral("work in progress"));

        m.replaceWithFiles({QUrl::fromLocalFile(path)}, 0);

        QCOMPARE(m.viewCount(), 2);          // the user's buffer survived
        QVERIFY2(m.viewFileUrl(0).isEmpty(),
                 "the untitled buffer is no longer the first tab");
        QCOMPARE(m.viewFileUrl(1), QUrl::fromLocalFile(path));
        m.setActiveIndex(0);
        QCOMPARE(m.activeDocument()->text(), QStringLiteral("work in progress"));
    }

    // An empty entry list must not close anything.
    void an_empty_list_changes_nothing()
    {
        EditorTabManager m;
        m.replaceWithFiles({}, 0);
        QCOMPARE(m.viewCount(), 1);
    }

    // Out-of-range active index must not leave no tab active.
    void an_out_of_range_active_index_is_clamped()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString a = writeFile(dir, QStringLiteral("a.txt"), QStringLiteral("AAA\n"));
        EditorTabManager m;

        m.replaceWithFiles({QUrl::fromLocalFile(a)}, 7);

        QCOMPARE(m.viewCount(), 1);
        QVERIFY(m.activeDocument());
        QCOMPARE(m.activeDocument()->url(), QUrl::fromLocalFile(a));
    }
};

QTEST_MAIN(TstRestore)
#include "tst_restore.moc"
