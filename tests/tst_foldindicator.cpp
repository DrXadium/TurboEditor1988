// SPDX-License-Identifier: GPL-3.0-or-later
// 2026-11-20: FoldIndicator - which lines carry the gutter fold marker? (pure).
//
// The gutter `▶`/`▼` sits on lines that OPEN a foldable block. Folding needs a
// block that spans >= 2 lines (Kate does not fold a one-liner), a block that
// actually closes, and an opener the highlighter owns (not a '{' inside a
// string or comment). Those rules already live in BlockFinder; this function
// just applies them to EVERY line and returns the qualifying ones, so each rule
// is a unit test rather than a pixel probe.
//
// See docs/superpowers/specs/2026-11-20-fold-indicator-design.md.
#include <QtTest>
#include <QVector>
#include <QString>
#include "src/editor/FoldIndicator.h"

using namespace FoldIndicator;

class TstFoldIndicator : public QObject
{
    Q_OBJECT

    // Mode-Nothing: no position is a string or a comment.
    static BlockFinder::SkipProbe nothingSkipped()
    {
        return [](int, int) { return false; };
    }

private slots:
    // Case 1: one top-level block -> its opener line only.
    void one_top_level_block_marks_its_opener_line()
    {
        const QVector<QString> lines = {
            QStringLiteral("int main(void) {"),
            QStringLiteral("    x();"),
            QStringLiteral("}"),
        };
        const QVector<int> got = foldableLines(lines, nothingSkipped());
        QCOMPARE(got, QVector<int>({ 0 }));
    }

    // Case 2: nested openers -> BOTH top-level and inner openers that span >=2 lines.
    void nested_openers_each_mark_their_own_line()
    {
        const QVector<QString> lines = {
            QStringLiteral("void f() {"),
            QStringLiteral("  if (a) {"),
            QStringLiteral("    y();"),
            QStringLiteral("  }"),
            QStringLiteral("}"),
        };
        const QVector<int> got = foldableLines(lines, nothingSkipped());
        QCOMPARE(got, QVector<int>({ 0, 1 }));
    }

    // Case 3: single-line block is NOT foldable (matches Kate).
    void a_single_line_block_is_not_marked()
    {
        const QVector<QString> lines = { QStringLiteral("int x = { 1 };") };
        const QVector<int> got = foldableLines(lines, nothingSkipped());
        QVERIFY2(got.isEmpty(), "a one-line block must not be marked");
    }

    // Case 4: unmatched opener (never closes) -> nothing.
    void an_unmatched_opener_is_not_marked()
    {
        const QVector<QString> lines = {
            QStringLiteral("void f() {"),
            QStringLiteral("    y();"),
        };
        const QVector<int> got = foldableLines(lines, nothingSkipped());
        QVERIFY2(got.isEmpty(), "an opener that never closes opens nothing");
    }

    // Case 5: skip probe suppresses an opener inside a string/comment.
    void an_opener_the_highlighter_owns_is_not_marked()
    {
        const QVector<QString> lines = {
            QStringLiteral("s = \"{\";"),   // the '{' is inside a string
            QStringLiteral("}"),
            QStringLiteral("int g() {"),
            QStringLiteral("  z();"),
            QStringLiteral("}"),
        };
        const auto skip = [](int line, int col) {
            return line == 0 && col == 5;   // the '{' inside the string literal
        };
        const QVector<int> got = foldableLines(lines, skip);
        QCOMPARE(got, QVector<int>({ 2 }));
    }
};

QTEST_MAIN(TstFoldIndicator)
#include "tst_foldindicator.moc"
