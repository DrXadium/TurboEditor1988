// SPDX-License-Identifier: GPL-3.0-or-later
// String colouring independent of the syntax mode (2026-11-20).
//
// A quoted span is found by SCANNING, not by asking the highlighter, so it works
// on a document whose mode is None (a .txt, an extension-less file, a fresh
// untitled tab) - those get no theme colours at all, which is what the user
// reported.
#include <QtTest>
#include <QVector>
#include <QString>
#include "src/editor/StringColorizer.h"

using namespace StringColorizer;

class TstStrings : public QObject
{
    Q_OBJECT

    // Probe that styles nothing: a document with mode None.
    static StyleProbe nothingStyled()
    {
        return [](int, int) { return false; };
    }

private Q_SLOTS:
    // A double-quoted span is found, with both ends.
    void finds_a_double_quoted_span()
    {
        const QVector<QString> lines{ QStringLiteral("x = \"hello\";") };
        const auto spans = findSpans(lines, 0, 0, nothingStyled());
        QCOMPARE(spans.size(), 1);
        QCOMPARE(spans.at(0).line, 0);
        QCOMPARE(spans.at(0).col, 4);       // the opening quote
        QCOMPARE(spans.at(0).endCol, 10);   // the closing quote (inclusive)
    }

    // Single quotes are strings too (char literals, shell, python).
    void finds_a_single_quoted_span()
    {
        const QVector<QString> lines{ QStringLiteral("c = 'z';") };
        const auto spans = findSpans(lines, 0, 0, nothingStyled());
        QCOMPARE(spans.size(), 1);
        QCOMPARE(spans.at(0).col, 4);
        QCOMPARE(spans.at(0).endCol, 6);
    }

    // Two spans on one line are two spans, not one long one.
    void finds_two_spans_on_one_line()
    {
        const QVector<QString> lines{ QStringLiteral("a(\"one\", 'two')") };
        const auto spans = findSpans(lines, 0, 0, nothingStyled());
        QCOMPARE(spans.size(), 2);
        QCOMPARE(spans.at(0).col, 2);
        QCOMPARE(spans.at(0).endCol, 6);
        QCOMPARE(spans.at(1).col, 9);
        QCOMPARE(spans.at(1).endCol, 13);
    }

    // An escaped quote does not close the span.
    void an_escaped_quote_does_not_close_the_span()
    {
        const QVector<QString> lines{ QStringLiteral("s = \"a\\\"b\";") };
        // s = " a \ " b " ;   -> the span runs from col 4 to the LAST quote
        const auto spans = findSpans(lines, 0, 0, nothingStyled());
        QCOMPARE(spans.size(), 1);
        QCOMPARE(spans.at(0).col, 4);
        QCOMPARE(spans.at(0).endCol, 9);
    }

    // An escaped backslash before the quote means the quote DOES close.
    void an_escaped_backslash_leaves_the_quote_closing()
    {
        const QVector<QString> lines{ QStringLiteral("p = \"c:\\\\\";") };
        // p = " c : \ \ " ;   -> closes at the quote after the doubled backslash
        const auto spans = findSpans(lines, 0, 0, nothingStyled());
        QCOMPARE(spans.size(), 1);
        QCOMPARE(spans.at(0).col, 4);
        QCOMPARE(spans.at(0).endCol, 9);
    }

    // A quote that never closes on its line is NOT a span: this is what keeps
    // prose ("don't", "it's") from turning the rest of a text file green.
    void an_unclosed_quote_is_ignored()
    {
        const QVector<QString> lines{ QStringLiteral("don't do this") };
        const auto spans = findSpans(lines, 0, 0, nothingStyled());
        QCOMPARE(spans.size(), 0);
    }

    // Same-line only: a quote opening on one line and closing on the next is
    // not a span (a real syntax mode already colours those).
    void a_span_never_crosses_a_line()
    {
        const QVector<QString> lines{ QStringLiteral("s = \"open"), QStringLiteral("close\";") };
        const auto spans = findSpans(lines, 0, 1, nothingStyled());
        QCOMPARE(spans.size(), 0);
    }

    // With a syntax mode the theme already colours strings, so those positions
    // are skipped - and a quote inside a COMMENT must not open a span either.
    void already_styled_positions_are_skipped()
    {
        // s = "hi";  // "not a string"
        // 0 1 2 3 45 6 7 8  9 0 1 2 3 456789012345678901234 567
        //           ^the real string      ^a QUOTED PAIR inside the comment
        const QVector<QString> lines{ QStringLiteral("s = \"hi\";  // \"not a string\"") };
        // The real string (cols 4..7) is styled by the mode; everything from the
        // comment marker (col 11) onward is a comment. A quoted pair inside a
        // comment must NOT be coloured, so no span may be reported at all.
        const StyleProbe probe = [](int line, int col) {
            Q_UNUSED(line);
            return (col >= 4 && col <= 7) || col >= 11;
        };
        const auto spans = findSpans(lines, 0, 0, probe);
        QCOMPARE(spans.size(), 0);
    }

    // Only spans STARTING inside the viewport are returned (upstream scopes the
    // work the same way for brackets).
    void only_spans_starting_in_the_viewport_are_returned()
    {
        const QVector<QString> lines{ QStringLiteral("a = \"one\""),
                                      QStringLiteral("b = \"two\""),
                                      QStringLiteral("c = \"three\"") };
        const auto spans = findSpans(lines, 1, 1, nothingStyled());
        QCOMPARE(spans.size(), 1);
        QCOMPARE(spans.at(0).line, 1);
    }

    // Double-quoted text keeps the theme's String green.
    void double_quoted_text_uses_the_string_green()
    {
        const QVector<QString> lines{ QStringLiteral("\"a\"") };
        const auto spans = findSpans(lines, 0, 0, nothingStyled());
        QCOMPARE(spans.size(), 1);
        QCOMPARE(spans.at(0).color, defaultColor());
        QCOMPARE(defaultColor().name(), QStringLiteral("#55ff55"));
    }

    // 2026-11-20 (user): single-quoted text gets its own colour so a nested pair
    // ("he said 'hi'") separates into two shades instead of one green run.
    void single_quoted_text_has_its_own_colour()
    {
        const QVector<QString> lines{ QStringLiteral("'b'") };
        const auto spans = findSpans(lines, 0, 0, nothingStyled());
        QCOMPARE(spans.size(), 1);
        QCOMPARE(spans.at(0).color.name(), QStringLiteral("#40a769"));
        QCOMPARE(spans.at(0).color, singleQuoteColor());
    }

    // The two kinds side by side keep their own colours.
    void the_two_quote_kinds_keep_their_own_colours()
    {
        const QVector<QString> lines{ QStringLiteral("\"a\" 'b'") };
        const auto spans = findSpans(lines, 0, 0, nothingStyled());
        QCOMPARE(spans.size(), 2);
        QCOMPARE(spans.at(0).color, defaultColor());
        QCOMPARE(spans.at(1).color, singleQuoteColor());
        QVERIFY2(spans.at(0).color != spans.at(1).color,
                 "the two quote kinds must not share a colour");
    }

    // THE nesting case, and the reason for the second colour: inside a
    // double-quoted string a single-quoted run takes the single-quote colour, so
    // the two are told apart at a glance. The span is split into non-overlapping
    // segments (an outer range covering the inner one would leave which colour
    // wins up to range ordering).
    void a_single_quoted_run_inside_a_double_quoted_string_is_its_own_colour()
    {
        // s = "he said 'hi'";
        // 0123456789...
        // "4 ......... '13 h i '16 "17
        const QVector<QString> lines{ QStringLiteral("s = \"he said 'hi'\";") };
        const auto spans = findSpans(lines, 0, 0, nothingStyled());
        QCOMPARE(spans.size(), 3);
        // "he said        -> double-quote green
        QCOMPARE(spans.at(0).col, 4);
        QCOMPARE(spans.at(0).endCol, 12);
        QCOMPARE(spans.at(0).color, defaultColor());
        // 'hi'            -> single-quote colour
        QCOMPARE(spans.at(1).col, 13);
        QCOMPARE(spans.at(1).endCol, 16);
        QCOMPARE(spans.at(1).color, singleQuoteColor());
        // closing "       -> back to green
        QCOMPARE(spans.at(2).col, 17);
        QCOMPARE(spans.at(2).endCol, 17);
        QCOMPARE(spans.at(2).color, defaultColor());
    }

    // And the mirror image: a double-quoted run inside a single-quoted string.
    void a_double_quoted_run_inside_a_single_quoted_string_is_green()
    {
        // h = '<a href="x">';
        // 0123456789012345678
        // '4 .......... "13 x "15
        const QVector<QString> lines{ QStringLiteral("h = '<a href=\"x\">';") };
        const auto spans = findSpans(lines, 0, 0, nothingStyled());
        QCOMPARE(spans.size(), 3);
        QCOMPARE(spans.at(0).color, singleQuoteColor());
        QCOMPARE(spans.at(1).col, 13);
        QCOMPARE(spans.at(1).endCol, 15);
        QCOMPARE(spans.at(1).color, defaultColor());
        QCOMPARE(spans.at(2).color, singleQuoteColor());
    }

    // Segments never overlap: each painted range must own its characters.
    void segments_never_overlap()
    {
        const QVector<QString> lines{ QStringLiteral("s = \"a 'b' c\" + 'd'") };
        const auto spans = findSpans(lines, 0, 0, nothingStyled());
        QVERIFY(spans.size() >= 4);
        for (int i = 1; i < spans.size(); ++i) {
            const auto& prev = spans.at(i - 1);
            const auto& cur = spans.at(i);
            QVERIFY2(cur.line != prev.line || cur.col > prev.endCol,
                     qPrintable(QStringLiteral("span %1 starts at %2, inside the "
                                               "previous span ending at %3")
                                    .arg(i).arg(cur.col).arg(prev.endCol)));
        }
    }

    // A one-character span ("") is still a span: two adjacent quotes.
    void an_empty_span_is_still_a_span()
    {
        const QVector<QString> lines{ QStringLiteral("s = \"\";") };
        const auto spans = findSpans(lines, 0, 0, nothingStyled());
        QCOMPARE(spans.size(), 1);
        QCOMPARE(spans.at(0).col, 4);
        QCOMPARE(spans.at(0).endCol, 5);
    }

    // A line index outside the document is not a crash.
    void an_out_of_range_viewport_is_safe()
    {
        const QVector<QString> lines{ QStringLiteral("x = \"a\"") };
        QCOMPARE(findSpans(lines, 0, 99, nothingStyled()).size(), 1);
        QCOMPARE(findSpans(lines, 5, 9, nothingStyled()).size(), 0);
        QCOMPARE(findSpans(QVector<QString>{}, 0, 0, nothingStyled()).size(), 0);
    }
};

QTEST_MAIN(TstStrings)
#include "tst_strings.moc"
