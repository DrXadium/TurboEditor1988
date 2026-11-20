// SPDX-License-Identifier: GPL-3.0-or-later
// 2026-11-20: View > Show / Hide Nonprintable Characters. Kate owns the drawing:
// every KTextEditor::View carries two checkable actions, view_show_whitespaces
// ("Show Whitespace") and view_non_printable_spaces ("Show Non-Printable
// Spaces"). One menu item drives BOTH, and the state is per view.
//
// Measured semantics (probe, offscreen): QAction::trigger() TOGGLES the checked
// flag (1 -> 0 -> 1); setChecked() moves the flag but is not the rendering path.
// So the implementation triggers until the state matches, never setChecked+trigger.
//
// TDD: written before setNonprintables/nonprintablesVisible exist.
#include <QtTest>
#include <QApplication>
#include <QAction>

#include <ktexteditor/document.h>
#include <ktexteditor/view.h>

#include "src/editor/EditorTabManager.h"

namespace {
QAction* viewAction(KTextEditor::View* v, const char* name)
{
    return v ? v->findChild<QAction*>(QString::fromLatin1(name)) : nullptr;
}
}

class TstNonprintable : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase() {
        if (qgetenv("QT_QPA_PLATFORM").isEmpty())
            qputenv("QT_QPA_PLATFORM", "offscreen");
    }

    void on_leaves_both_kate_actions_checked() {
        EditorTabManager m;
        m.newDocument();
        KTextEditor::View* v = m.activeView();
        QAction* ws = viewAction(v, "view_show_whitespaces");
        QAction* np = viewAction(v, "view_non_printable_spaces");
        QVERIFY2(ws, "view_show_whitespaces missing");
        QVERIFY2(np, "view_non_printable_spaces missing");

        m.setNonprintables(true);

        QVERIFY(ws->isChecked());
        QVERIFY(np->isChecked());
        QVERIFY(m.nonprintablesVisible());
    }

    void off_leaves_both_kate_actions_unchecked() {
        EditorTabManager m;
        m.newDocument();
        KTextEditor::View* v = m.activeView();
        QAction* ws = viewAction(v, "view_show_whitespaces");
        QAction* np = viewAction(v, "view_non_printable_spaces");

        m.setNonprintables(true);
        m.setNonprintables(false);

        QVERIFY(!ws->isChecked());
        QVERIFY(!np->isChecked());
        QVERIFY(!m.nonprintablesVisible());
    }

    void setting_the_same_state_twice_does_not_flip_back() {
        EditorTabManager m;
        m.newDocument();
        KTextEditor::View* v = m.activeView();
        QAction* ws = viewAction(v, "view_show_whitespaces");

        m.setNonprintables(true);
        m.setNonprintables(true);

        QVERIFY(ws->isChecked());
    }

    // The manager's constructor always creates one tab, so "no document" is not
    // a reachable state; a fresh manager must simply start with the markers off.
    // (Probed: a document created AFTER a toggle starts off too - the state is
    // per document, and one view per document is this app's model.)
    void fresh_manager_starts_off() {
        EditorTabManager m;
        QVERIFY(m.activeView());
        QVERIFY(!m.nonprintablesVisible());
    }

    void state_follows_the_active_tab() {
        EditorTabManager m;                    // index 0: the constructor's tab
        m.newDocument();                       // index 1: the one we toggle
        m.setNonprintables(true);
        QVERIFY(m.nonprintablesVisible());

        m.newDocument();                       // index 2: fresh tab, markers off
        QVERIFY(!m.nonprintablesVisible());

        m.setActiveIndex(1);                   // back to the toggled tab
        QVERIFY(m.nonprintablesVisible());

        m.setActiveIndex(0);                   // the untouched first tab
        QVERIFY(!m.nonprintablesVisible());
    }
};

QTEST_MAIN(TstNonprintable)
#include "tst_nonprintable.moc"
