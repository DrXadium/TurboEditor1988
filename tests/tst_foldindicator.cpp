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
#include <QAction>
#include <QImage>
#include <QByteArray>
#include <QPixmap>
#include <QIcon>
#include <KTextEditor/Document>
#include <KTextEditor/View>
#include "src/editor/FoldIndicator.h"
#include "src/editor/EditorTabManager.h"

using namespace FoldIndicator;

class TstFoldIndicator : public QObject
{
    Q_OBJECT

    void initTestCase()
    {
        if (qgetenv("QT_QPA_PLATFORM").isEmpty())
            qputenv("QT_QPA_PLATFORM", "offscreen");
    }

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

    // --- Coupling to the Show Block Markers toggle (Task 2) -----------------
    // The gutter `▶` is a BLOCK MARKER, so it appears only when the one toggle
    // is on, and `icon-bar` (the strip that draws it) follows the same flag.

    // A: markers OFF -> no marks, no strip. (Clean gutter by default.)
    void markers_off_leaves_no_marks_and_no_strip()
    {
        EditorTabManager tabs;
        tabs.activeDocument()->setHighlightingMode(QStringLiteral("C++"));
        tabs.activeDocument()->setText(QStringLiteral(
            "void f()\n"
            "{\n"
            "    x();\n"
            "}\n"));
        tabs.setBlockMarkersVisible(false);
        QVERIFY2(tabs.activeDocument()->marks().isEmpty(),
                 "markers off must leave no gutter marks");
        QCOMPARE(tabs.activeView()->configValue(QStringLiteral("icon-bar")).toBool(),
                 false);
    }

    // B: markers ON with a foldable block -> mark on the opener, strip on.
    void markers_on_marks_the_fold_opener_and_turns_the_strip_on()
    {
        EditorTabManager tabs;
        tabs.activeDocument()->setHighlightingMode(QStringLiteral("C++"));
        tabs.activeDocument()->setText(QStringLiteral(
            "void f()\n"
            "{\n"
            "    x();\n"
            "}\n"));
        tabs.setBlockMarkersVisible(true);
        QVERIFY2(tabs.activeDocument()->marks().contains(1),
                 "the block opener (line 1 = '{') must carry a mark");
        QCOMPARE(tabs.activeView()->configValue(QStringLiteral("icon-bar")).toBool(),
                 true);
    }

    // C: markers ON but a mode-less document -> NO marks (folding needs a mode).
    void markers_on_a_modeless_document_gets_no_marks()
    {
        EditorTabManager tabs;   // untitled, highlighting mode = None by default
        tabs.activeDocument()->setText(QStringLiteral(
            "void f()\n"
            "{\n"
            "    x();\n"
            "}\n"));
        tabs.setBlockMarkersVisible(true);
        QVERIFY2(tabs.activeDocument()->marks().isEmpty(),
                 "a mode-less document must never carry fold marks");
    }

    // D: collapsing the block swaps the opener's `▶` for a `▼`.
    void collapsing_the_block_swaps_the_gutter_glyph()
    {
        EditorTabManager tabs;
        tabs.activeDocument()->setHighlightingMode(QStringLiteral("C++"));
        tabs.activeDocument()->setText(QStringLiteral(
            "void f()\n"
            "{\n"
            "    x();\n"
            "}\n"));
        tabs.setBlockMarkersVisible(true);
        QVERIFY(tabs.activeDocument()->marks().contains(1));

        const auto iconPixels = [](const QIcon& ic) {
            QPixmap pm = ic.pixmap(16, 16);
            QImage img = pm.toImage().convertToFormat(QImage::Format_ARGB32);
            QByteArray bytes;
            const uchar* bits = img.constBits();
            bytes.append(reinterpret_cast<const char*>(bits),
                         static_cast<int>(img.sizeInBytes()));
            return bytes;
        };
        const QByteArray before = iconPixels(tabs.activeDocument()->markIcon(
            KTextEditor::Document::Bookmark));

        tabs.activeView()->setCursorPosition(KTextEditor::Cursor(1, 0));
        tabs.collapseBlock();   // folded now -> the mark's icon must become `▼`

        const QByteArray after = iconPixels(tabs.activeDocument()->markIcon(
            KTextEditor::Document::Bookmark));
        QVERIFY2(before != after,
                 "collapsing must swap the ▶ gutter glyph for a ▼");
    }

    // E: the glyph is document-global (setMarkIcon is per mark TYPE, not per
    // line), so with TWO foldable blocks, collapsing the FIRST must still flip
    // the glyph to ▼ - not be overwritten by the last (unfolded) block's state.
    void collapsing_one_of_two_blocks_still_flips_the_global_glyph()
    {
        EditorTabManager tabs;
        tabs.activeDocument()->setHighlightingMode(QStringLiteral("C++"));
        tabs.activeDocument()->setText(QStringLiteral(
            "void a()\n"
            "{\n"
            "    x();\n"
            "}\n"
            "void b()\n"
            "{\n"
            "    y();\n"
            "}\n"));
        tabs.setBlockMarkersVisible(true);
        // Both openers are foldable: lines 1 ('{' of a) and 5 ('{' of b).
        QVERIFY(tabs.activeDocument()->marks().contains(1));
        QVERIFY(tabs.activeDocument()->marks().contains(5));

        const auto iconPixels = [](const QIcon& ic) {
            QPixmap pm = ic.pixmap(16, 16);
            QImage img = pm.toImage().convertToFormat(QImage::Format_ARGB32);
            QByteArray bytes;
            bytes.append(reinterpret_cast<const char*>(img.constBits()),
                         static_cast<int>(img.sizeInBytes()));
            return bytes;
        };
        const QByteArray before = iconPixels(tabs.activeDocument()->markIcon(
            KTextEditor::Document::Bookmark));

        // Collapse the FIRST block only (line 1); leave the second (line 5) open.
        tabs.activeView()->setCursorPosition(KTextEditor::Cursor(1, 0));
        tabs.collapseBlock();

        const QByteArray after = iconPixels(tabs.activeDocument()->markIcon(
            KTextEditor::Document::Bookmark));
        QVERIFY2(before != after,
                 "collapsing the first of two blocks must flip the global ▶ to ▼");
    }

    // F: the marker state is per-view and follows the active tab (the same
    // contract setNonprintables/word wrap honour), so the menu checkbox - which
    // mirrors blockMarkersVisible() on activeViewChanged - never lies.
    void marker_state_follows_the_active_tab()
    {
        EditorTabManager tabs;                // index 0: constructor's tab
        tabs.newDocument();                   // index 1: the one we toggle
        tabs.activeDocument()->setHighlightingMode(QStringLiteral("C++"));
        tabs.setBlockMarkersVisible(true);
        QVERIFY2(tabs.blockMarkersVisible(), "the toggled tab must be on");

        tabs.newDocument();                   // index 2: fresh tab, markers off
        QVERIFY2(!tabs.blockMarkersVisible(), "a fresh tab must start off");

        tabs.setActiveIndex(1);               // back to the toggled tab
        QVERIFY2(tabs.blockMarkersVisible(), "state must follow back to tab 1");

        tabs.setActiveIndex(0);               // the untouched first tab
        QVERIFY2(!tabs.blockMarkersVisible(), "tab 0 was never toggled");
    }
};

QTEST_MAIN(TstFoldIndicator)
#include "tst_foldindicator.moc"
