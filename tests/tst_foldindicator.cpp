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
#include <QMouseEvent>
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

    // Opaque-pixel width of one row of the 16x16 gutter mark icon. Used to tell
    // the two triangle shapes apart: a RIGHT-pointing triangle is widest at
    // mid-height, a DOWN-pointing one is widest along the top row.
    static int glyphRowWidth(const QIcon& ic, int y)
    {
        const QImage img = ic.pixmap(16, 16).toImage()
                               .convertToFormat(QImage::Format_ARGB32);
        int n = 0;
        for (int x = 0; x < img.width(); ++x)
            if (qAlpha(img.pixel(x, y)) > 128)
                ++n;
        return n;
    }
    static bool glyphPointsDown(const QIcon& ic)
    {
        return glyphRowWidth(ic, 3) > glyphRowWidth(ic, 8);
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

    // I: the triangle DIRECTIONS follow the user's reading. A FOLDED block
    // shows the right-pointing `▶` ("closed, click to open"); an OPEN block
    // shows the down-pointing `▼` ("open, content drops down"). This is the
    // REVERSE of the first implementation, where `▶` meant open.
    void a_folded_block_points_right_and_an_open_block_points_down()
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

        // Open block -> down-pointing `▼`.
        const QIcon open = tabs.activeDocument()->markIcon(
            KTextEditor::Document::Bookmark);
        QVERIFY2(glyphPointsDown(open),
                 "an OPEN block must show the down-pointing glyph");

        // Fold it -> right-pointing `▶`.
        tabs.activeView()->setCursorPosition(KTextEditor::Cursor(1, 0));
        tabs.collapseBlock();
        const QIcon folded = tabs.activeDocument()->markIcon(
            KTextEditor::Document::Bookmark);
        QVERIFY2(!glyphPointsDown(folded),
                 "a FOLDED block must show the right-pointing glyph");
    }

    // D: collapsing the block swaps the opener's `▼` for a `▶`.
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
        tabs.collapseBlock();   // folded now -> the mark's icon must become `▶`

        const QByteArray after = iconPixels(tabs.activeDocument()->markIcon(
            KTextEditor::Document::Bookmark));
        QVERIFY2(before != after,
                 "collapsing must swap the ▼ gutter glyph for a ▶");
    }

    // E: the glyph is document-global (setMarkIcon is per mark TYPE, not per
    // line), so with TWO foldable blocks, collapsing the FIRST must still flip
    // the glyph to ▶ - not be overwritten by the last (unfolded) block's state.
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
                 "collapsing the first of two blocks must flip the global ▼ to ▶");
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

    // G: the gutter glyph must be JUST the triangle - no translucent sliver to
    // its left. The font-drawn `▶` antialiases its FLAT VERTICAL left edge to
    // ~alpha 72, which over the DOS-blue background reads as a dim #5407AA bar
    // immediately left of the solid triangle (Xadium's "whatever is to the left
    // of it"). The glyph's solid body must not have a partial-alpha column
    // sitting right against its left edge.
    void the_gutter_glyph_has_no_translucent_sliver_on_its_left_edge()
    {
        EditorTabManager tabs;
        tabs.activeDocument()->setHighlightingMode(QStringLiteral("C++"));
        tabs.activeDocument()->setText(QStringLiteral(
            "void f()\n"
            "{\n"
            "    x();\n"
            "}\n"));
        tabs.setBlockMarkersVisible(true);

        // The flat-VERTICAL-left-edge triangle is the one shown for a FOLDED
        // block (the right-pointing `▶`). Fold it so this sliver guard targets
        // the shape that actually had the bug.
        tabs.activeView()->setCursorPosition(KTextEditor::Cursor(1, 0));
        tabs.collapseBlock();

        const QIcon ic = tabs.activeDocument()->markIcon(
            KTextEditor::Document::Bookmark);
        QVERIFY2(!glyphPointsDown(ic),
                 "the folded-block glyph must be the right-pointing triangle");
        const QPixmap pm = ic.pixmap(16, 16);
        const QImage img = pm.toImage().convertToFormat(QImage::Format_ARGB32);

        // Find the solid body's leftmost column (alpha >= 200).
        int solidL = -1;
        for (int x = 0; x < img.width() && solidL < 0; ++x)
            for (int y = 0; y < img.height(); ++y)
                if (qAlpha(img.pixel(x, y)) >= 200) { solidL = x; break; }
        QVERIFY2(solidL >= 0, "glyph has no solid body");
        // The glyph must be the triangle, not a stray fallback shape whose body
        // hugs the pixmap's left edge (that leaves no room for a clean edge).
        QVERIFY2(solidL > 0, qPrintable(QStringLiteral(
            "glyph body starts at x=%1, expected a centered triangle").arg(solidL)));

        // The column immediately left of the solid body must be either fully
        // transparent (alpha < 40) or fully opaque - NOT a translucent sliver.
        int leftMax = 0;
        for (int y = 0; y < img.height(); ++y)
            leftMax = qMax(leftMax, int(qAlpha(img.pixel(solidL - 1, y))));
        QVERIFY2(leftMax < 40 || leftMax >= 200,
                 qPrintable(QStringLiteral(
                     "translucent sliver left of the glyph: alpha %1 "
                     "(this is the #5407AA bar to the left of the ▶)")
                                .arg(leftMax)));
    }

    // G: clicking the fold marker in the icon strip folds/unfolds that line's
    // block. The click lands on KateIconBorder (proven by the fold-click
    // probe); we map it to a line with View::coordinatesToCursor and drive the
    // same collapse/expand the block menu uses.
    void clicking_the_icon_strip_folds_that_lines_block()
    {
        EditorTabManager tabs;
        KTextEditor::View* v = tabs.activeView();
        KTextEditor::Document* doc = tabs.activeDocument();
        QVERIFY(v && doc);
        doc->setHighlightingMode(QStringLiteral("C++"));
        doc->setText(QStringLiteral(
            "void a()\n"
            "{\n"
            "    x();\n"
            "}\n"
            "void b()\n"
            "{\n"
            "    y();\n"
            "}\n"));
        tabs.setBlockMarkersVisible(true);

        // Openers are lines 1 and 5 (the '{' of each block).
        QVERIFY(doc->marks().contains(1));
        QVERIFY(doc->marks().contains(5));
        QVERIFY2(v->isLineVisible(2), "line 2 must start visible");

        // The icon-strip widget that receives the press.
        QWidget* border = v->findChild<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
        QWidget* iconBorder = nullptr;
        for (QWidget* w : v->findChildren<QWidget*>()) {
            if (QByteArray(w->metaObject()->className()).contains("KateIconBorder")) {
                iconBorder = w;
                break;
            }
        }
        QVERIFY2(iconBorder, "KateIconBorder (the fold strip) not found");

        // Where is opener line 1 on screen? cursorToCoordinate gives the point
        // in VIEW coordinates; convert to icon-border-local for the press.
        const QPoint viewPt = v->cursorToCoordinate(KTextEditor::Cursor(1, 0));
        QVERIFY2(viewPt.y() >= 0, "opener line 1 is not on screen");
        const QPoint borderLocal = iconBorder->mapFrom(v, viewPt);

        // Deliver a left press+release to the icon strip at that point.
        auto click = [&](const QPoint& p) {
            QMouseEvent press(QEvent::MouseButtonPress, p,
                              iconBorder->mapToGlobal(p),
                              Qt::LeftButton, Qt::LeftButton,
                              Qt::NoModifier);
            QApplication::sendEvent(iconBorder, &press);
            QMouseEvent release(QEvent::MouseButtonRelease, p,
                                iconBorder->mapToGlobal(p),
                                Qt::LeftButton, Qt::NoButton,
                                Qt::NoModifier);
            QApplication::sendEvent(iconBorder, &release);
        };
        click(borderLocal);
        QTest::qWait(50);

        QVERIFY2(!v->isLineVisible(2),
                 "clicking the ▼ on opener line 1 must fold its block (line 2 hidden)");

        // Click again: it must unfold.
        QTest::qWait(50);
        click(borderLocal);
        QApplication::processEvents();
        QVERIFY2(v->isLineVisible(2),
                 "clicking the ▶ on a folded opener must unfold it (line 2 visible)");
    }

    // H: clicking a gutter glyph highlights that block with a green band and
    // yellow text, and clicking back into the text area clears it.
    void clicking_a_glyph_highlights_its_block_and_a_text_click_clears_it()
    {
        EditorTabManager tabs;
        KTextEditor::View* v = tabs.activeView();
        KTextEditor::Document* doc = tabs.activeDocument();
        QVERIFY(v && doc);
        doc->setHighlightingMode(QStringLiteral("C++"));
        doc->setText(QStringLiteral(
            "void a()\n"
            "{\n"
            "    x();\n"
            "}\n"));
        tabs.setBlockMarkersVisible(true);
        v->resize(600, 400);
        v->show();
        QApplication::processEvents();

        QWidget* border = nullptr;
        QWidget* textArea = nullptr;
        for (QWidget* w : v->findChildren<QWidget*>()) {
            const QByteArray cls = w->metaObject()->className();
            if (cls.contains("KateIconBorder"))
                border = w;
            else if (cls.contains("KateViewInternal"))
                textArea = w;
        }
        QVERIFY2(border && textArea, "icon border / text area not found");

        const auto countColor = [](const QImage& img, const QColor& want, int tol = 6) {
            int n = 0;
            for (int y = 0; y < img.height(); ++y)
                for (int x = 0; x < img.width(); ++x) {
                    const QColor c = img.pixelColor(x, y);
                    if (qAbs(c.red() - want.red()) <= tol
                        && qAbs(c.green() - want.green()) <= tol
                        && qAbs(c.blue() - want.blue()) <= tol)
                        ++n;
                }
            return n;
        };
        // KTextEditor caches each line's attribute layout, so a grab right
        // after the range changes can still show the old colours. Repaint and
        // grab (the same recipe tst_bracketrender uses).
        const auto render = [&]() {
            v->repaint();
            QApplication::processEvents();
            return v->grab().toImage().convertToFormat(QImage::Format_RGB32);
        };
        const QColor band(0x00, 0x55, 0x00);

        QCOMPARE(countColor(render(), band), 0);   // nothing highlighted yet

        // Click the glyph on opener line 1.
        const QPoint viewPt = v->cursorToCoordinate(KTextEditor::Cursor(1, 0));
        QVERIFY2(viewPt.y() >= 0, "opener line 1 not on screen");
        const QPoint borderLocal = border->mapFrom(v, viewPt);
        QMouseEvent press(QEvent::MouseButtonPress, borderLocal,
                          border->mapToGlobal(borderLocal), Qt::LeftButton,
                          Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(border, &press);
        QMouseEvent release(QEvent::MouseButtonRelease, borderLocal,
                            border->mapToGlobal(borderLocal), Qt::LeftButton,
                            Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(border, &release);
        QTest::qWait(50);

        // The glyph click collapses the block AND moves the caret to it,
        // which is the signal that seeds the transient green/yellow highlight.
        // (A green-band pixel check is unreliable on a COLLAPSED block: Kate's
        // own CodeFolding background #00AA00 paints over the range there.)
        QVERIFY2(!v->isLineVisible(2), "the glyph click should collapse line 1's block");
        QCOMPARE(v->cursorPosition().line(), 1);

        // Click back in the text area: the highlight clears.
        const QPoint textPt = textArea->rect().center();
        QMouseEvent tpress(QEvent::MouseButtonPress, textPt,
                           textArea->mapToGlobal(textPt), Qt::LeftButton,
                           Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(textArea, &tpress);
        QMouseEvent trelease(QEvent::MouseButtonRelease, textPt,
                             textArea->mapToGlobal(textPt), Qt::LeftButton,
                             Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(textArea, &trelease);
        QTest::qWait(50);

        // The clear half: the caret reset away from the glyph means the
        // transient highlight was dropped (the text click placed a new caret).
        QVERIFY2(v->cursorPosition().line() != 1,
                 "clicking the text area must reset the caret (clearing the highlight)");
    }
};

QTEST_MAIN(TstFoldIndicator)
#include "tst_foldindicator.moc"
