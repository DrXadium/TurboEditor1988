// SPDX-License-Identifier: GPL-3.0-or-later
// 2026-11-20: BlockFinder - "which block OPENS on this line?" (pure, no widgets).
//
// The context menu's Collapse/Expand Block commands and the block highlight all
// need the extent of the block under the caret, and Kate exposes NO public
// fold-region API (measured: only the actions folding_toplevel,
// folding_expandtoplevel, folding_toggle_current, folding_toggle_in_current and
// config keys exist). So the extent is computed here, and every rule below is a
// unit test rather than a pixel probe.
//
// Decisions from chat: "on a block" means the caret is on the line that OPENS it
// (Kate's folding-marker rule), and the extents come from our own matcher.
#include <QtTest>
#include <QVector>
#include <QString>
#include "src/editor/BlockFinder.h"

using namespace BlockFinder;

class TstBlockFinder : public QObject
{
    Q_OBJECT

    // Nothing is a string or a comment: the mode-None case.
    static SkipProbe nothingSkipped()
    {
        return [](int, int) { return false; };
    }

    static QVector<QString> linesOf(const QString& text)
    {
        return text.split(QLatin1Char('\n'));
    }

private Q_SLOTS:
    // The plain case: a brace block opening on the caret's line.
    void finds_a_brace_block()
    {
        const auto lines = linesOf(QStringLiteral(
            "int main(void) {\n"
            "    return 0;\n"
            "}\n"));
        const Block b = findBlockStartingAt(lines, 0, nothingSkipped());
        QVERIFY(b.valid);
        QCOMPARE(b.startLine, 0);
        QCOMPARE(b.endLine, 2);
    }

    // The LAST opener on the line wins: `int main(void) {` must fold the brace
    // body, not the `(void)` argument list - that is what Kate folds.
    void the_last_opener_on_the_line_wins()
    {
        const auto lines = linesOf(QStringLiteral(
            "int main(void) {\n"
            "    return 0;\n"
            "}\n"));
        const Block b = findBlockStartingAt(lines, 0, nothingSkipped());
        QVERIFY(b.valid);
        // The brace, not the paren. "int main(void) {" puts the paren at column 8
        // and the brace at column 15, so naming both makes the intent explicit:
        // this must fail if the matcher ever picks the argument list.
        QCOMPARE(b.startCol, 15);
        QVERIFY2(b.startCol != 8, "the matcher picked the (void) argument list");
    }

    // But a paren that is the only opener still works (a call spanning lines).
    void finds_a_paren_block_spanning_lines()
    {
        const auto lines = linesOf(QStringLiteral(
            "foo(a,\n"
            "    b,\n"
            "    c);\n"));
        const Block b = findBlockStartingAt(lines, 0, nothingSkipped());
        QVERIFY(b.valid);
        QCOMPARE(b.startCol, 3);
        QCOMPARE(b.endLine, 2);
    }

    // Nesting is counted, so the block ends at the MATCHING closer, not the first.
    void nested_braces_match_the_right_closer()
    {
        const auto lines = linesOf(QStringLiteral(
            "void f() {\n"
            "    if (x) {\n"
            "        y();\n"
            "    }\n"
            "}\n"));
        const Block b = findBlockStartingAt(lines, 0, nothingSkipped());
        QVERIFY(b.valid);
        QCOMPARE(b.endLine, 4);        // the outer brace, not the inner one on line 3
    }

    // A single-line `{}` is not worth folding and Kate does not fold it.
    void a_single_line_block_is_not_a_block()
    {
        const auto lines = linesOf(QStringLiteral(
            "if (x) { y(); }\n"
            "z();\n"));
        const Block b = findBlockStartingAt(lines, 0, nothingSkipped());
        QVERIFY(!b.valid);
    }

    // An opener that never closes opens nothing.
    void an_unclosed_opener_is_not_a_block()
    {
        const auto lines = linesOf(QStringLiteral(
            "void f() {\n"
            "    y();\n"));
        const Block b = findBlockStartingAt(lines, 0, nothingSkipped());
        QVERIFY(!b.valid);
    }

    // No opener at all.
    void a_plain_line_has_no_block()
    {
        const auto lines = linesOf(QStringLiteral("just some text\nmore text\n"));
        QVERIFY(!findBlockStartingAt(lines, 0, nothingSkipped()).valid);
    }

    // An opener inside a COMMENT opens nothing (the probe says it is owned).
    void a_brace_inside_a_comment_opens_nothing()
    {
        const auto lines = linesOf(QStringLiteral(
            "// not a block {\n"
            "x();\n"
            "}\n"));
        const SkipProbe comment = [](int line, int col) {
            return line == 0;                  // the whole comment line
        };
        QVERIFY(!findBlockStartingAt(lines, 0, comment).valid);
    }

    // An opener inside a STRING opens nothing.
    void a_brace_inside_a_string_opens_nothing()
    {
        const auto lines = linesOf(QStringLiteral(
            "s = \"{ not a block\";\n"
            "x();\n"
            "}\n"));
        const SkipProbe str = [](int line, int col) {
            return line == 0 && col >= 4 && col <= 19;
        };
        QVERIFY(!findBlockStartingAt(lines, 0, str).valid);
    }

    // ---- HTML ----

    // The user's case: an HTML container element.
    void finds_an_html_container_block()
    {
        const auto lines = linesOf(QStringLiteral(
            "<div class=\"card\">\n"
            "    <p>hi</p>\n"
            "</div>\n"));
        const Block b = findBlockStartingAt(lines, 0, nothingSkipped());
        QVERIFY(b.valid);
        QCOMPARE(b.startLine, 0);
        QCOMPARE(b.endLine, 2);
    }

    // Same-tag nesting must match the OUTER closing tag.
    void nested_html_tags_match_the_outer_closer()
    {
        const auto lines = linesOf(QStringLiteral(
            "<div>\n"
            "  <div>\n"
            "    inner\n"
            "  </div>\n"
            "</div>\n"));
        const Block b = findBlockStartingAt(lines, 0, nothingSkipped());
        QVERIFY(b.valid);
        QCOMPARE(b.endLine, 4);
    }

    // A void element never opens a block.
    void a_void_element_is_not_a_block()
    {
        const auto lines = linesOf(QStringLiteral(
            "<br>\n"
            "<p>text</p>\n"));
        QVERIFY(!findBlockStartingAt(lines, 0, nothingSkipped()).valid);
    }

    // Nor does a self-closing tag.
    void a_self_closing_tag_is_not_a_block()
    {
        const auto lines = linesOf(QStringLiteral(
            "<img src=\"a.png\" />\n"
            "<p>text</p>\n"));
        QVERIFY(!findBlockStartingAt(lines, 0, nothingSkipped()).valid);
    }

    // A DOCTYPE is not an element.
    void a_doctype_is_not_a_block()
    {
        const auto lines = linesOf(QStringLiteral(
            "<!DOCTYPE html>\n"
            "<html>\n"
            "</html>\n"));
        QVERIFY(!findBlockStartingAt(lines, 0, nothingSkipped()).valid);
    }

    // A tag inside a string is text, not structure.
    void a_tag_inside_a_string_opens_nothing()
    {
        const auto lines = linesOf(QStringLiteral(
            "s = \"<div>\";\n"
            "x();\n"
            "</div>\n"));
        const SkipProbe str = [](int line, int col) {
            return line == 0 && col >= 4 && col <= 10;
        };
        QVERIFY(!findBlockStartingAt(lines, 0, str).valid);
    }

    // An unclosed HTML element opens nothing.
    void an_unclosed_html_element_is_not_a_block()
    {
        const auto lines = linesOf(QStringLiteral(
            "<div>\n"
            "  <p>hi</p>\n"));
        QVERIFY(!findBlockStartingAt(lines, 0, nothingSkipped()).valid);
    }

    // The caret line itself matters: the block opens on ITS line, so asking
    // about a line inside the block finds nothing.
    void a_line_inside_the_block_opens_nothing()
    {
        const auto lines = linesOf(QStringLiteral(
            "<div>\n"
            "  <p>hi</p>\n"
            "</div>\n"));
        QVERIFY(!findBlockStartingAt(lines, 1, nothingSkipped()).valid);
    }

    // Out-of-range lines must be safe.
    void an_out_of_range_line_is_safe()
    {
        const auto lines = linesOf(QStringLiteral("<div>\n</div>\n"));
        QVERIFY(!findBlockStartingAt(lines, 99, nothingSkipped()).valid);
        QVERIFY(!findBlockStartingAt(QVector<QString>{}, 0, nothingSkipped()).valid);
    }

    // A realistic mixed file: the caret on the <section> line finds the section.
    void finds_the_section_in_a_realistic_html_file()
    {
        const auto lines = linesOf(QStringLiteral(
            "<!DOCTYPE html>\n"
            "<html>\n"
            "<body>\n"
            "  <section class=\"hero\">\n"
            "    <h1>Title</h1>\n"
            "  </section>\n"
            "</body>\n"
            "</html>\n"));
        const Block b = findBlockStartingAt(lines, 3, nothingSkipped());
        QVERIFY(b.valid);
        QCOMPARE(b.startLine, 3);
        QCOMPARE(b.endLine, 5);
    }
};

QTEST_MAIN(TstBlockFinder)
#include "tst_blockfinder.moc"
