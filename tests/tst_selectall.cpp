// SPDX-License-Identifier: GPL-3.0-or-later
// 2026-11-20: Edit > Select All / Deselect All. Kate exposes no select-all
// action in this build (every view action was enumerated; only Emmet/script
// selectors exist), so both commands are ours: selectAllActive() covers the
// WHOLE document regardless of any current selection, deselectActive() clears
// the selection WITHOUT moving the caret (measured: removeSelection() leaves
// the cursor where it was).
//
// TDD: written before selectAllActive/deselectActive exist.
#include <QtTest>
#include <QApplication>

#include <ktexteditor/document.h>
#include <ktexteditor/view.h>

#include "src/editor/EditorTabManager.h"

class TstSelectAll : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase() {
        if (qgetenv("QT_QPA_PLATFORM").isEmpty())
            qputenv("QT_QPA_PLATFORM", "offscreen");
    }

    void select_all_covers_the_whole_document() {
        EditorTabManager m;
        m.newDocument();
        KTextEditor::View* v = m.activeView();
        QVERIFY(v);
        v->document()->setText(QStringLiteral("alpha\nbeta\ngamma"));

        m.selectAllActive();

        QVERIFY(v->selection());
        QCOMPARE(v->selectionRange().start(), KTextEditor::Cursor(0, 0));
        QCOMPARE(v->selectionRange().end(), KTextEditor::Cursor(2, 5));
    }

    void select_all_replaces_a_smaller_selection() {
        EditorTabManager m;
        m.newDocument();
        KTextEditor::View* v = m.activeView();
        v->document()->setText(QStringLiteral("alpha\nbeta\ngamma"));
        v->setSelection(KTextEditor::Range(0, 0, 0, 2));

        m.selectAllActive();

        QCOMPARE(v->selectionRange().start(), KTextEditor::Cursor(0, 0));
        QCOMPARE(v->selectionRange().end(), KTextEditor::Cursor(2, 5));
    }

    void deselect_clears_the_selection_and_keeps_the_caret() {
        EditorTabManager m;
        m.newDocument();
        KTextEditor::View* v = m.activeView();
        v->document()->setText(QStringLiteral("alpha\nbeta\ngamma"));
        v->setCursorPosition(KTextEditor::Cursor(1, 2));

        m.selectAllActive();
        QVERIFY(v->selection());

        m.deselectActive();

        QVERIFY(!v->selection());
        QCOMPARE(v->cursorPosition(), KTextEditor::Cursor(1, 2));
    }

    void empty_document_is_safe() {
        EditorTabManager m;
        m.newDocument();
        KTextEditor::View* v = m.activeView();
        QVERIFY(v);

        m.selectAllActive();
        m.deselectActive();

        QVERIFY(!v->selection());
    }
};

QTEST_MAIN(TstSelectAll)
#include "tst_selectall.moc"
