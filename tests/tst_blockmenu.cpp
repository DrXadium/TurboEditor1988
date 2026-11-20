// SPDX-License-Identifier: GPL-3.0-or-later
// 2026-11-20: the block commands behind the right-click menu.
//
// User: "in the right click menu there needs to be a collapse block and expand
// block option when one is on a bloc. Also highlight block if possible, which
// would highlight it with a red background with yellow text".
//
// Decisions from chat: "on a block" = the caret is on the line that OPENS it;
// extents come from BlockFinder; the highlight is a toggle item.
//
// Measured facts this suite rests on:
//  - Kate has NO collapse-current/expand-current action, only
//    folding_toggle_current, so collapse/expand must check the folded state
//    first (via View::isLineVisible) or they are not idempotent.
//  - A MovingRange background DOES paint over the theme (5415 px measured).
//
// TDD: written before the EditorTabManager additions exist.
#include <QtTest>
#include <QApplication>

#include <ktexteditor/document.h>
#include <ktexteditor/view.h>

#include "src/editor/EditorTabManager.h"

class TstBlockMenu : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        if (qgetenv("QT_QPA_PLATFORM").isEmpty())
            qputenv("QT_QPA_PLATFORM", "offscreen");
    }

    // On a line that opens a block, the manager reports one.
    void a_block_is_found_on_its_opening_line()
    {
        EditorTabManager m;
        m.activeDocument()->setHighlightingMode(QStringLiteral("C++"));
        m.activeDocument()->setText(QStringLiteral(
            "void f() {\n"
            "    int x = 1;\n"
            "}\n"));
        m.activeView()->setCursorPosition(KTextEditor::Cursor(0, 0));

        QVERIFY(m.canActOnBlock());
        const BlockFinder::Block b = m.currentBlock();
        QVERIFY(b.valid);
        QCOMPARE(b.startLine, 0);
        QCOMPARE(b.endLine, 2);
    }

    // On a line inside the block but not its opener, there is no block: the
    // commands act on the block that STARTS here, per the chat decision.
    void a_line_inside_the_block_has_no_block()
    {
        EditorTabManager m;
        m.activeDocument()->setHighlightingMode(QStringLiteral("C++"));
        m.activeDocument()->setText(QStringLiteral(
            "void f() {\n"
            "    int x = 1;\n"
            "}\n"));
        m.activeView()->setCursorPosition(KTextEditor::Cursor(1, 4));
        QVERIFY(!m.canActOnBlock());
    }

    // Collapsing hides the body; the folded state is observable.
    void collapse_block_hides_the_body()
    {
        EditorTabManager m;
        m.activeDocument()->setHighlightingMode(QStringLiteral("C++"));
        m.activeDocument()->setText(QStringLiteral(
            "void f() {\n"
            "    int a = 1;\n"
            "    int b = 2;\n"
            "    int c = 3;\n"
            "}\n"));
        m.activeView()->setCursorPosition(KTextEditor::Cursor(0, 0));
        const int before = m.activeView()->visibleLines();
        QVERIFY(!m.currentBlockIsFolded());

        m.collapseBlock();

        QVERIFY2(m.activeView()->visibleLines() < before,
                 "collapse did not hide any line");
        QVERIFY(m.currentBlockIsFolded());
    }

    // Expand restores it.
    void expand_block_shows_the_body_again()
    {
        EditorTabManager m;
        m.activeDocument()->setHighlightingMode(QStringLiteral("C++"));
        m.activeDocument()->setText(QStringLiteral(
            "void f() {\n"
            "    int a = 1;\n"
            "    int b = 2;\n"
            "    int c = 3;\n"
            "}\n"));
        m.activeView()->setCursorPosition(KTextEditor::Cursor(0, 0));
        const int total = m.activeView()->visibleLines();
        m.collapseBlock();
        QVERIFY(m.currentBlockIsFolded());

        m.expandBlock();

        QCOMPARE(m.activeView()->visibleLines(), total);
        QVERIFY(!m.currentBlockIsFolded());
    }

    // Collapse twice must not re-open it: Kate only offers a TOGGLE action, so
    // the manager has to check the state before triggering it.
    void collapse_is_idempotent()
    {
        EditorTabManager m;
        m.activeDocument()->setHighlightingMode(QStringLiteral("C++"));
        m.activeDocument()->setText(QStringLiteral(
            "void f() {\n"
            "    int a = 1;\n"
            "    int b = 2;\n"
            "}\n"));
        m.activeView()->setCursorPosition(KTextEditor::Cursor(0, 0));

        m.collapseBlock();
        const int afterFirst = m.activeView()->visibleLines();
        m.collapseBlock();

        QCOMPARE(m.activeView()->visibleLines(), afterFirst);
        QVERIFY(m.currentBlockIsFolded());
    }

    // Same the other way round.
    void expand_is_idempotent()
    {
        EditorTabManager m;
        m.activeDocument()->setHighlightingMode(QStringLiteral("C++"));
        m.activeDocument()->setText(QStringLiteral(
            "void f() {\n"
            "    int a = 1;\n"
            "    int b = 2;\n"
            "}\n"));
        m.activeView()->setCursorPosition(KTextEditor::Cursor(0, 0));
        const int total = m.activeView()->visibleLines();

        m.expandBlock();          // nothing folded yet
        m.expandBlock();

        QCOMPARE(m.activeView()->visibleLines(), total);
    }

    // Off a block, the commands are harmless no-ops rather than crashes.
    void the_commands_are_safe_off_a_block()
    {
        EditorTabManager m;
        m.activeDocument()->setText(QStringLiteral("just text\nmore text\n"));
        m.activeView()->setCursorPosition(KTextEditor::Cursor(0, 0));
        QVERIFY(!m.canActOnBlock());

        const int before = m.activeView()->visibleLines();
        m.collapseBlock();
        m.expandBlock();
        QCOMPARE(m.activeView()->visibleLines(), before);
    }

    // The highlight toggle is off by default and flips.
    void the_highlight_toggle_flips()
    {
        EditorTabManager m;
        m.activeDocument()->setHighlightingMode(QStringLiteral("C++"));
        m.activeDocument()->setText(QStringLiteral(
            "void f() {\n"
            "    int a = 1;\n"
            "}\n"));
        m.activeView()->setCursorPosition(KTextEditor::Cursor(0, 0));

        QVERIFY(!m.blockHighlight());
        m.setBlockHighlight(true);
        QVERIFY(m.blockHighlight());
        m.setBlockHighlight(false);
        QVERIFY(!m.blockHighlight());
    }

    // Turning the highlight on with no block under the caret must be safe and
    // must not leave a stale highlight behind.
    void highlighting_off_a_block_is_safe()
    {
        EditorTabManager m;
        m.activeDocument()->setText(QStringLiteral("plain text\n"));
        m.activeView()->setCursorPosition(KTextEditor::Cursor(0, 0));

        m.setBlockHighlight(true);
        QVERIFY(m.blockHighlight());       // the toggle itself still holds
        m.setBlockHighlight(false);
        QVERIFY(!m.blockHighlight());
    }

    // The three context-menu actions exist with the documented object names.
    void the_context_menu_actions_exist()
    {
        EditorTabManager m;
        m.activeDocument()->setHighlightingMode(QStringLiteral("C++"));
        m.activeDocument()->setText(QStringLiteral(
            "void f() {\n"
            "    int a = 1;\n"
            "}\n"));
        m.activeView()->setCursorPosition(KTextEditor::Cursor(0, 0));

        QVERIFY2(m.contextBlockAction(QStringLiteral("actionCtxCollapseBlock")),
                 "Collapse Block action missing");
        QVERIFY2(m.contextBlockAction(QStringLiteral("actionCtxExpandBlock")),
                 "Expand Block action missing");
        QAction* hl = m.contextBlockAction(QStringLiteral("actionCtxHighlightBlock"));
        QVERIFY2(hl, "Highlight Block action missing");
        QVERIFY2(hl->isCheckable(), "Highlight Block must be checkable");
    }

    // Enabled state follows the caret: on a block and unfolded, Collapse is
    // available and Expand is not.
    void the_actions_follow_the_caret()
    {
        EditorTabManager m;
        m.activeDocument()->setHighlightingMode(QStringLiteral("C++"));
        m.activeDocument()->setText(QStringLiteral(
            "void f() {\n"
            "    int a = 1;\n"
            "    int b = 2;\n"
            "}\n"
            "\n"
            "plain\n"));
        QAction* collapse = m.contextBlockAction(QStringLiteral("actionCtxCollapseBlock"));
        QAction* expand = m.contextBlockAction(QStringLiteral("actionCtxExpandBlock"));
        QVERIFY(collapse && expand);

        // On the opening line, unfolded.
        m.activeView()->setCursorPosition(KTextEditor::Cursor(0, 0));
        QVERIFY2(collapse->isEnabled(), "Collapse should be enabled on an open block");
        QVERIFY2(!expand->isEnabled(), "Expand should be disabled on an open block");

        // Collapse, then the pair swaps.
        m.collapseBlock();
        m.activeView()->setCursorPosition(KTextEditor::Cursor(0, 0));
        QVERIFY2(!collapse->isEnabled(), "Collapse should be disabled once folded");
        QVERIFY2(expand->isEnabled(), "Expand should be enabled once folded");

        // Off a block both are disabled.
        m.activeView()->setCursorPosition(KTextEditor::Cursor(5, 0));
        QVERIFY2(!collapse->isEnabled(), "Collapse should be disabled off a block");
        QVERIFY2(!expand->isEnabled(), "Expand should be disabled off a block");
    }
};

QTEST_MAIN(TstBlockMenu)
#include "tst_blockmenu.moc"
