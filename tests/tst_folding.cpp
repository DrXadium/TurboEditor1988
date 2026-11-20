// SPDX-License-Identifier: GPL-3.0-or-later
// 2026-11-20: the folding ("nesting") menu - Collapse All / Expand All / Toggle
// Current Node, driving Kate's own per-view actions.
//
// Probed before writing this: the actions exist and DO fold
// (folding_toplevel 10 visible -> 4, folding_expandtoplevel -> 10,
// folding_toggle_current 10 -> 8), folding state is per VIEW, and a document
// whose syntax mode is None folds nothing at all. Folding state is queryable
// through View::visibleLines()/isLineVisible(), so these are number assertions,
// not pixel probes.
//
// TDD: written before collapseAll/expandAll/toggleCurrentNode/canFold exist.
#include <QtTest>
#include <QApplication>
#include <QElapsedTimer>

#include <ktexteditor/document.h>
#include <ktexteditor/view.h>

#include "src/editor/EditorTabManager.h"

namespace {
// Folding regions come from the syntax highlighting pass, which is not
// instantaneous: spin the event loop until it has run.
void pump(int ms = 400)
{
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
}

// A C function with a nested block - the classic foldable shape.
QString sample()
{
    return QStringLiteral("#include <stdio.h>\n"
                          "int main(void) {\n"
                          "    int n = 0;\n"
                          "    if (n == 0) {\n"
                          "        printf(\"zero\\n\");\n"
                          "        n = 1;\n"
                          "    }\n"
                          "    return 0;\n"
                          "}\n");
}
} // namespace

class TstFolding : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        if (qgetenv("QT_QPA_PLATFORM").isEmpty())
            qputenv("QT_QPA_PLATFORM", "offscreen");
    }

    void collapse_all_folds_the_top_level_nodes()
    {
        EditorTabManager m;
        m.newDocument();
        KTextEditor::View* v = m.activeView();
        QVERIFY(v);
        v->document()->setText(sample());
        v->document()->setHighlightingMode(QStringLiteral("C++"));
        pump();

        const int before = v->visibleLines();
        QCOMPARE(before, v->document()->lines());   // nothing folded yet

        m.collapseAll();

        QVERIFY2(v->visibleLines() < before,
                 qPrintable(QStringLiteral("collapseAll folded nothing (%1 -> %2)")
                                .arg(before).arg(v->visibleLines())));
    }

    void expand_all_restores_every_line()
    {
        EditorTabManager m;
        m.newDocument();
        KTextEditor::View* v = m.activeView();
        v->document()->setText(sample());
        v->document()->setHighlightingMode(QStringLiteral("C++"));
        pump();

        m.collapseAll();
        QVERIFY(v->visibleLines() < v->document()->lines());

        m.expandAll();

        QCOMPARE(v->visibleLines(), v->document()->lines());
    }

    // Expand All is the DEFAULT state, so it must be harmless when nothing is
    // folded - and it must never add or remove text.
    void expand_all_is_harmless_when_nothing_is_folded()
    {
        EditorTabManager m;
        m.newDocument();
        KTextEditor::View* v = m.activeView();
        const QString text = sample();
        v->document()->setText(text);
        v->document()->setHighlightingMode(QStringLiteral("C++"));
        pump();

        m.expandAll();

        QCOMPARE(v->visibleLines(), v->document()->lines());
        QCOMPARE(v->document()->text(), text);
    }

    void toggle_current_node_folds_the_block_under_the_caret()
    {
        EditorTabManager m;
        m.newDocument();
        KTextEditor::View* v = m.activeView();
        v->document()->setText(sample());
        v->document()->setHighlightingMode(QStringLiteral("C++"));
        pump();
        v->setCursorPosition(KTextEditor::Cursor(3, 0));   // the `if` line
        pump(100);

        const int before = v->visibleLines();
        m.toggleCurrentNode();
        const int folded = v->visibleLines();
        QVERIFY2(folded < before,
                 qPrintable(QStringLiteral("toggleCurrentNode folded nothing (%1 -> %2)")
                                .arg(before).arg(folded)));
        // The line the caret sits on stays visible; its body goes.
        QVERIFY(v->isLineVisible(3));

        m.toggleCurrentNode();                             // and back
        QCOMPARE(v->visibleLines(), before);
    }

    // Folding is per VIEW, and every tab is its own view - so the user's "it
    // should be effective per tab" comes for free. This pins it.
    void folding_a_tab_does_not_fold_another_tab()
    {
        EditorTabManager m;
        m.newDocument();
        KTextEditor::View* first = m.activeView();
        first->document()->setText(sample());
        first->document()->setHighlightingMode(QStringLiteral("C++"));
        pump();

        m.newDocument();
        KTextEditor::View* second = m.activeView();
        QVERIFY(second != first);
        second->document()->setText(sample());
        second->document()->setHighlightingMode(QStringLiteral("C++"));
        pump();

        m.collapseAll();                       // applies to the ACTIVE tab only

        QVERIFY(second->visibleLines() < second->document()->lines());
        QCOMPARE(first->visibleLines(), first->document()->lines());
    }

    // A document with no syntax mode has no folding regions: probed, the actions
    // do nothing at all (10 visible -> 10). The menu must say so rather than
    // offer a command that cannot work.
    void can_fold_is_false_without_a_syntax_mode()
    {
        EditorTabManager m;
        m.newDocument();
        KTextEditor::View* v = m.activeView();
        v->document()->setText(sample());
        v->document()->setHighlightingMode(QStringLiteral("None"));
        pump();

        QVERIFY(!m.canFold());
        const int before = v->visibleLines();
        m.collapseAll();                       // must be a harmless no-op
        QCOMPARE(v->visibleLines(), before);
    }

    void can_fold_is_true_for_a_foldable_mode()
    {
        EditorTabManager m;
        m.newDocument();
        KTextEditor::View* v = m.activeView();
        v->document()->setText(sample());
        v->document()->setHighlightingMode(QStringLiteral("C++"));
        pump();

        QVERIFY(m.canFold());
    }

    // No active view must not crash the commands.
    void the_commands_are_safe_with_no_view()
    {
        EditorTabManager m;
        m.collapseAll();
        m.expandAll();
        m.toggleCurrentNode();
        QVERIFY(!m.canFold());
    }
};

QTEST_MAIN(TstFolding)
#include "tst_folding.moc"
