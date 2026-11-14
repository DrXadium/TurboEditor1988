// SPDX-License-Identifier: GPL-3.0-or-later
// Task 12: Turbo Vision window-frame contract test.
// Proves the editor host paints a double-line frame: left + right vertical
// edges, bottom edge (the TabStrip supplies the top edge/title line).
#include <QtTest>
#include <QImage>
#include <QColor>
#include <QMenuBar>
#include <QStatusBar>
#include "src/shell/ShellWindow.h"
#include "src/shell/TabStrip.h"
#include "src/shell/BottomBand.h"
#include "src/shell/TextScrollBar.h"
#include "src/shell/RightScrollBar.h"
#include "src/shell/TvFrame.h"
#include <QLabel>

namespace {
const QColor kFrameGray(0xAA, 0xAA, 0xAA);
const QColor kBgBlue(0x00, 0x00, 0xAA);
const QColor kFrameWhite(0xFF, 0xFF, 0xFF);   // TV-revision spec 3: white double line

bool isGray(const QColor& c)
{
    return qAbs(c.red() - 0xAA) <= 30 && qAbs(c.green() - 0xAA) <= 30 &&
           qAbs(c.blue() - 0xAA) <= 30;
}

// Spec 3 supersedes gray: the double-line window frame is #FFFFFF.
bool isFrameWhite(const QColor& c)
{
    return qAbs(c.red() - 0xFF) <= 24 && qAbs(c.green() - 0xFF) <= 24 &&
           qAbs(c.blue() - 0xFF) <= 24;
}

bool isBgBlue(const QColor& c)
{
    return qAbs(c.red() - 0x00) <= 40 && qAbs(c.green() - 0x00) <= 40 &&
           qAbs(c.blue() - 0xAA) <= 40;
}

bool isGreen(const QColor& c)
{
    return c.green() > 120 && c.red() < 100 && c.blue() < 100;
}

// The bottom border row's two rule rows: the band rows with the most white
// pixels. Returns false when no rule row is found at all.
bool ruleRows(const QImage& img, const QPoint& origin, int bandW, int bandH,
              int* upper, int* lower)
{
    QList<int> rows;
    for (int y = 0; y < bandH; ++y) {
        int n = 0;
        for (int x = 0; x < bandW; ++x)
            if (isFrameWhite(img.pixelColor(origin.x() + x, origin.y() + y)))
                ++n;
        if (n > bandW / 2)
            rows << y;
    }
    if (rows.size() < 2)
        return false;
    *upper = rows.first();
    *lower = rows.last();
    return true;
}
} // namespace

class TstFrame : public QObject
{
    Q_OBJECT
private slots:
    // The TabStrip (title line) paints the green close + zoom boxes at the
    // top-left / top-right of the window.
    void title_line_has_close_and_zoom_boxes() {
        ShellWindow w;
        w.resize(800, 600);
        TabStrip* ts = w.tabStrip();
        QVERIFY(ts);
        ts->setTabs(QStringList() << "untitled", 0);
        const QImage img = w.grab().toImage();
        QVERIFY(!img.isNull());

        // Close/zoom boxes are in TabStrip-local coords; map to the shell grab.
        const QPoint stripOrigin = ts->mapTo(&w, QPoint(0, 0));
        auto sampleGreenLocal = [&](const QRect& box, const QPoint* pts, int n) {
            for (int i = 0; i < n; ++i) {
                const QPoint local = pts[i];
                const QPoint shell = stripOrigin + box.topLeft() + local;
                if (shell.x() < 0 || shell.y() < 0 ||
                    shell.x() >= img.width() || shell.y() >= img.height())
                    continue;
                const QColor c = img.pixelColor(shell);
                if (c.green() > 110 && c.red() < 130 && c.blue() < 130)
                    return true;
            }
            return false;
        };
        const QRect cb = ts->closeBoxRect(), zb = ts->zoomBoxRect();
        const QPoint closePts[] = { QPoint(2, 2), QPoint(cb.width() - 4, 2),
                                    QPoint(2, cb.height() - 4) };
        // Zoom face is window blue with green ↕ strokes — sample the stem
        // and the diagonal joints (spec 1: green glyphs, no black ink).
        const QPoint zoomPts[] = { QPoint(zb.width() / 2, 4),
                                   QPoint(zb.width() / 2, zb.height() - 5),
                                   QPoint(zb.width() / 2 - 3, 4) };
        const bool closeGreen = sampleGreenLocal(cb, closePts, 3);
        const bool zoomGreen = sampleGreenLocal(zb, zoomPts, 3);
        QVERIFY2(closeGreen && zoomGreen,
                 qPrintable(QStringLiteral("green boxes missing (close=%1 zoom=%2)")
                                .arg(closeGreen).arg(zoomGreen)));
    }

    // The editor host paints left + right vertical frame edges (gray) along
    // its sides, with blue between.
    // Task 4: the DOS double-line frame box has all FOUR corners.
    // Task 19: the top two are the ╔ ╗ of the DOCUMENT's border row (the band
    // below the tab row), not the tab row's ends - the tab row carries no rule
    // of its own any more. Sample the actual frame-box corners (not the
    // menubar/status bar, which are also gray and would mask a missing corner).
    void frame_has_all_four_corners() {
        ShellWindow w;
        w.resize(400, 300);
        w.show();
        QCoreApplication::processEvents();
        TabStrip* ts = w.tabStrip();
        BottomBand* band = w.bottomBand();
        QWidget* titleRow = w.findChild<QWidget*>(QStringLiteral("editorTitleRow"));
        QVERIFY(ts && band && titleRow);
        const QImage img = w.grab().toImage();
        QVERIFY(!img.isNull());

        const QPoint titleOrigin = titleRow->mapTo(&w, QPoint(0, 0));
        const QPoint bOrigin = band->mapTo(&w, QPoint(0, 0));

        // Frame-box corners in shell coordinates:
        //   top-left/top-right: the document border row's first/last cells.
        //   bottom-left/bottom-right: the BOTTOM BAND's BORDER ROW — the lower
        //   of the band's two rows (the scrollbar owns the upper one). The
        //   bottom-right corner is GREEN by spec, so it is matched separately.
        const int borderY = band->height() - band->fontMetrics().height() / 2;
        const int cornerY = titleRow->height() / 2;
        const QPoint corners[4] = {
            titleOrigin + QPoint(3, cornerY),                       // top-left
            titleOrigin + QPoint(titleRow->width() - 4, cornerY),    // top-right
            bOrigin + QPoint(3, borderY),                           // bottom-left
            bOrigin + QPoint(band->width() - 4, borderY)            // bottom-right
        };
        const char* names[4] = { "top-left", "top-right", "bottom-left", "bottom-right" };

        for (int i = 0; i < 4; ++i) {
            bool found = false;
            const QPoint c = corners[i];
            for (int dy = -4; dy <= 4 && !found; ++dy)
                for (int dx = -4; dx <= 4 && !found; ++dx) {
                    const int x = c.x() + dx, y = c.y() + dy;
                    if (x < 0 || y < 0 || x >= img.width() || y >= img.height())
                        continue;
                    const QColor c2 = img.pixelColor(x, y);
                    if (isFrameWhite(c2) || (i == 3 && isGreen(c2)))
                        found = true;
                }
            QVERIFY2(found, qPrintable(QStringLiteral("frame %1 corner missing WHITE box at (%2,%3)")
                                           .arg(names[i]).arg(c.x()).arg(c.y())));
        }
    }

    void editor_host_paints_left_frame_edge() {
        ShellWindow w;
        w.resize(800, 600);
        const QImage img = w.grab().toImage();
        QVERIFY(!img.isNull());
        const int midY = img.height() / 2;

        // Walk inward from the left edge: find a WHITE frame pixel (spec 3).
        bool leftWhite = false;
        for (int x = 0; x < 12 && x < img.width(); ++x)
            if (isFrameWhite(img.pixelColor(x, midY))) { leftWhite = true; break; }

        QVERIFY2(leftWhite, "no WHITE left frame edge found");
        // Right border: NO white verticals — spec 1 hands that column to the
        // 1-cell vertical scrollbar (covered by tst_scrollbars).
        for (int x = img.width() - 12; x < img.width(); ++x)
            QVERIFY2(!isFrameWhite(img.pixelColor(x, midY)),
                     qPrintable(QStringLiteral("WHITE right frame edge still painted at x=%1")
                                    .arg(x)));
    }

    // TV-revision2 spec 1: no black anywhere inside the window — between the
    // menu bar and the status line every pixel is painted (no voids/gutters).
    void no_black_inside_window() {
        ShellWindow w;
        w.resize(800, 600);
        w.show();
        QCoreApplication::processEvents();
        const QImage img = w.grab().toImage();
        QVERIFY(!img.isNull());

        const int y0 = w.menuBar()->mapTo(&w, QPoint(0, 0)).y() + w.menuBar()->height();
        const QPoint stl = w.statusBar()->mapTo(&w, QPoint(0, 0));
        const int y1 = stl.y() - 1;
        QVERIFY(y1 >= y0);
        int black = 0;
        for (int y = y0; y <= y1; ++y)
            for (int x = 0; x < img.width(); ++x)
                if (img.pixelColor(x, y) == QColor(0, 0, 0))
                    ++black;
        QString samples;
        int shown = 0;
        for (int y = y0; y <= y1 && shown < 8; ++y)
            for (int x = 0; x < img.width() && shown < 8; ++x)
                if (img.pixelColor(x, y) == QColor(0, 0, 0)) {
                    samples += QStringLiteral(" (%1,%2)").arg(x).arg(y);
                    ++shown;
                }
        QVERIFY2(black == 0,
                 qPrintable(QStringLiteral("pure black inside window: %1 px (rows %2..%3), first:%4")
                                .arg(black).arg(y0).arg(y1).arg(samples)));
    }
    // TV spec: the bottom border row is a ═ rule that BREAKS for the cursor
    // position, rather than the text sitting on top of a solid rule.
    // The left border is drawn with the ║ GLYPH, so each stroke is a full
    // character stroke. Two 1-px hairlines (the old TvFrame::doubleV) measured
    // exactly 2 px per row and read as thin next to the real IDE.
    void left_border_is_a_thick_double_rule()
    {
        ShellWindow w;
        w.resize(600, 400);
        w.show();
        QCoreApplication::processEvents();
        auto* host = w.findChild<QWidget*>(QStringLiteral("editorHost"));
        QVERIFY2(host, "editorHost widget not found");
        const QImage img = host->grab().toImage();
        QVERIFY2(!img.isNull() && img.width() > 100, "editorHost did not render");

        int widest = 0;
        for (int y = img.height() / 4; y < img.height() / 2; ++y) {
            int n = 0;
            for (int x = 0; x < 16 && x < img.width(); ++x)
                if (isFrameWhite(img.pixelColor(x, y)))
                    ++n;
            widest = qMax(widest, n);
        }
        QVERIFY2(widest >= 3,
                 qPrintable(QStringLiteral(
                     "left border is only %1 px wide - two hairlines, not a ║ glyph")
                                .arg(widest)));
    }

    // The bottom-left corner is the ╚ glyph: the vertical stroke comes down the
    // left of the cell and the horizontal stroke runs out to its right, so the
    // two borders actually join instead of meeting at a seam.
    void bottom_left_corner_joins_vertical_and_horizontal()
    {
        ShellWindow w;
        w.resize(600, 400);
        w.show();
        QCoreApplication::processEvents();
        BottomBand* band = w.bottomBand();
        QVERIFY(band);
        const QImage img = band->grab().toImage();
        QVERIFY2(!img.isNull() && img.width() > 100, "bottom band did not render");
        const int cw = qMax(6, band->fontMetrics().horizontalAdvance(QLatin1Char('M')));
        const int h = img.height();

        // The cell holds BOTH strokes: the vertical stem on its left half and
        // the horizontal rule running out across its right half. (The rule sits
        // in the middle of the cell, not at the bottom - scan the whole cell.)
        bool stem = false, rule = false;
        for (int x = 0; x < cw / 2 && x < img.width(); ++x)
            for (int y = 0; y < h; ++y)
                if (isFrameWhite(img.pixelColor(x, y)))
                    stem = true;
        for (int x = cw / 2; x < cw && x < img.width(); ++x)
            for (int y = 0; y < h; ++y)
                if (isFrameWhite(img.pixelColor(x, y)))
                    rule = true;
        QVERIFY2(stem, "corner cell has no vertical stroke - ╚ is missing its stem");
        QVERIFY2(rule, "corner cell has no horizontal stroke - ╚ is not joining ═");
    }

    // The bottom rule BREAKS for the position text (space, "L:C", space) and
    // then continues until it reaches the horizontal scrollbar.
    void bottom_rule_breaks_around_the_cursor_position()
    {
        ShellWindow w;
        w.resize(600, 400);
        w.show();
        QCoreApplication::processEvents();
        BottomBand* band = w.bottomBand();
        QVERIFY(band);
        const QImage img = band->grab().toImage();
        QVERIFY2(!img.isNull() && img.width() > 100, "bottom band did not render");
        const int bw = img.width(), bh = img.height();

        // The rule now only spans the left part of the row (the scrollbar owns
        // the rest), so scan for the rule inside that span rather than the
        // whole band.
        const int scanTo = qMin(bw, 260);
        int ruleY = -1, best = 0;
        for (int y = 0; y < bh; ++y) {
            int n = 0;
            for (int x = 0; x < scanTo; ++x)
                if (isFrameWhite(img.pixelColor(x, y)))
                    ++n;
            if (n > best) { best = n; ruleY = y; }
        }
        // The rule only spans the left stretch (the scrollbar owns the rest of
        // the row), so a few cells of white is the expected magnitude.
        QVERIFY2(ruleY >= 0 && best >= 16, "no bottom rule found left of the scrollbar");

        bool gap = false;
        for (int x = 40; x < scanTo && !gap; ++x)
            if (isBgBlue(img.pixelColor(x, ruleY)))
                gap = true;
        QVERIFY2(gap, "bottom rule is unbroken - the cursor position must interrupt it");
    }

    // The ║ column must run all the way to the EditorHost's bottom edge, or it
    // stops short of the band and the corner is left floating. (tileV used to
    // draw only WHOLE cells, leaving up to a cell of blue at the bottom.)
    void left_border_reaches_the_bottom_edge_of_the_editor_host()
    {
        ShellWindow w;
        w.resize(600, 400);
        w.show();
        QCoreApplication::processEvents();
        auto* host = w.findChild<QWidget*>(QStringLiteral("editorHost"));
        QVERIFY2(host, "editorHost widget not found");
        const QImage img = host->grab().toImage();
        QVERIFY2(!img.isNull() && img.height() > 50, "editorHost did not render");
        const int cw = qMax(6, host->fontMetrics().horizontalAdvance(QLatin1Char('M')));

        int lastWhiteRow = -1;
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < cw; ++x)
                if (isFrameWhite(img.pixelColor(x, y))) { lastWhiteRow = y; break; }

        QVERIFY2(lastWhiteRow >= img.height() - 2,
                 qPrintable(QStringLiteral("left border stops at row %1 of %2 - it must "
                                           "reach the bottom edge to meet the corner")
                                .arg(lastWhiteRow).arg(img.height())));
    }

    // The ═ rule continues well past the position text before the scrollbar
    // starts - it does not stop one cell after the numbers.
    void bottom_rule_extends_to_the_scrollbar_start()
    {
        ShellWindow w;
        w.resize(900, 400);
        w.show();
        QCoreApplication::processEvents();
        BottomBand* band = w.bottomBand();
        QVERIFY(band && band->horizontalBar());
        const QImage img = band->grab().toImage();
        QVERIFY2(!img.isNull() && img.width() > 300, "bottom band did not render");
        const int bw = img.width();
        const int cw = qMax(6, band->fontMetrics().horizontalAdvance(QLatin1Char('M')));

        // Rightmost white pixel on the rule row, left of the scrollbar.
        const int hbarLeft = band->horizontalBar()->geometry().left();
        int ruleRight = -1;
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < qMin(bw, hbarLeft); ++x)
                if (isFrameWhite(img.pixelColor(x, y)))
                    ruleRight = qMax(ruleRight, x);

        QVERIFY2(ruleRight >= 160,
                 qPrintable(QStringLiteral("bottom rule ends at x=%1 - it must run on "
                                           "towards the scrollbar at %2")
                                .arg(ruleRight).arg(hbarLeft)));
        // The rule runs right up to the scrollbar's left edge.
        QVERIFY2(ruleRight >= hbarLeft - 3,
                 qPrintable(QStringLiteral("bottom rule ends at x=%1, expected to reach "
                                           "the scrollbar at %2")
                                .arg(ruleRight).arg(hbarLeft)));
    }

    // The right corner is BRIGHT green and is preceded by a light ─ in the same
    // green, reading "─┘" as the reference does.
    void bottom_right_corner_is_bright_green_after_a_light_rule()
    {
        ShellWindow w;
        w.resize(900, 400);
        w.show();
        QCoreApplication::processEvents();
        BottomBand* band = w.bottomBand();
        QVERIFY(band);
        const QImage img = band->grab().toImage();
        QVERIFY2(!img.isNull() && img.width() > 300, "bottom band did not render");
        const int bw = img.width();
        const int cw = qMax(6, band->fontMetrics().horizontalAdvance(QLatin1Char('M')));

        const auto isBrightGreen = [](const QColor& c) {
            return c.green() >= 220 && c.red() < 100 && c.blue() < 100;
        };

        int bright = 0;
        for (int y = 0; y < img.height(); ++y)
            for (int x = qMax(0, bw - cw); x < bw; ++x)
                if (isBrightGreen(img.pixelColor(x, y)))
                    ++bright;
        QVERIFY2(bright > 0, "bottom-right corner is not BRIGHT green");

        int leftCellGreen = 0;
        for (int y = 0; y < img.height(); ++y)
            for (int x = qMax(0, bw - 2 * cw); x < bw - cw; ++x) {
                const QColor c = img.pixelColor(x, y);
                if (c.green() > 120 && c.red() < 100 && c.blue() < 100)
                    ++leftCellGreen;
            }
        QVERIFY2(leftCellGreen > 0,
                 "no ─ to the left of the corner - the row must read ─┘ in green");
    }

    // The bottom-right corner is the GREEN ┘ that joins the horizontal
    // scrollbar row to the vertical scrollbar column.
    void bottom_right_corner_is_green()
    {
        ShellWindow w;
        w.resize(600, 400);
        w.show();
        QCoreApplication::processEvents();
        BottomBand* band = w.bottomBand();
        QVERIFY(band);
        const QImage img = band->grab().toImage();
        QVERIFY2(!img.isNull() && img.width() > 100, "bottom band did not render");
        const int bw = img.width();

        const int cw = qMax(6, band->fontMetrics().horizontalAdvance(QLatin1Char('M')));
        int green = 0;
        for (int y = 0; y < img.height(); ++y)
            for (int x = qMax(0, bw - cw - 1); x < bw; ++x)
                if (isGreen(img.pixelColor(x, y)))
                    ++green;
        QVERIFY2(green > 0, "bottom-right corner is not green");
    }

    // The bottom-right corner must DOVETAIL with the right-border scrollbar
    // column: the corner glyph has to occupy the SAME character cell as the
    // column, so its ┘ stem is centred under the column it joins.
    //
    // Measured bug: the column was a hardcoded 12 px while the character grid
    // is cellW (8 px at the bundled 16 px VGA face) - one and a half cells. The
    // ┘ was drawn in an 8 px cell inside a 12 px column, so its stem floated
    // 7 px in from the column's left edge and the corner never lined up.
    void bottom_right_corner_dovetails_with_the_right_scrollbar_column()
    {
        ShellWindow w;
        w.resize(900, 400);
        w.show();
        QCoreApplication::processEvents();

        auto* bar = w.findChild<RightScrollBar*>(QStringLiteral("rightScrollBar"));
        QVERIFY2(bar, "rightScrollBar (the window's right border column) not found");

        BottomBand* band = w.bottomBand();
        QVERIFY(band && band->horizontalBar());
        const int cw = qMax(6, band->fontMetrics().horizontalAdvance(QLatin1Char('M')));
        const int bw = band->width();
        QVERIFY2(bw > 300, "bottom band has no sane width");

        // (1) The border column must be exactly ONE character cell wide.
        QVERIFY2(bar->width() == cw,
                 qPrintable(QStringLiteral("right border column is %1 px wide, but one "
                                           "character cell is %2 px - the corner glyph "
                                           "can never dovetail with a column off the grid")
                                .arg(bar->width()).arg(cw)));

        // (2) It must occupy the corner glyph's cell: the band's last cell.
        const QPoint colTL = bar->mapTo(&w, QPoint(0, 0));
        const QPoint bandTL = band->mapTo(&w, QPoint(0, 0));
        const int cornerCellLeft = bandTL.x() + bw - cw;
        QVERIFY2(colTL.x() == cornerCellLeft,
                 qPrintable(QStringLiteral("right column starts at x=%1 but the corner "
                                           "glyph's cell starts at x=%2 - they are not "
                                           "the same cell")
                                .arg(colTL.x()).arg(cornerCellLeft)));

        // (3) The green ┘ stem must be centred under the column.
        const QImage img = band->grab().toImage();
        QVERIFY2(!img.isNull() && img.width() > 300, "bottom band did not render");
        int stemLeft = -1, stemRight = -1;
        for (int x = bw - cw; x < bw; ++x) {
            int greenRows = 0;
            for (int y = 0; y < img.height(); ++y)
                if (isGreen(img.pixelColor(x, y)))
                    ++greenRows;
            if (greenRows >= 4) {
                if (stemLeft < 0)
                    stemLeft = x;
                stemRight = x;
            }
        }
        QVERIFY2(stemLeft >= 0, "corner cell has no green vertical stem");

        // Compare in ABSOLUTE coordinates (the stem was found in band-local px).
        const int stemAbsLeft = bandTL.x() + stemLeft;
        const int stemAbsRight = bandTL.x() + stemRight;
        const int stemCentre2 = stemAbsLeft + stemAbsRight;  // 2 * centre
        const int colCentre2 = 2 * colTL.x() + cw - 1;       // 2 * centre (absolute)
        QVERIFY2(qAbs(stemCentre2 - colCentre2) <= 2,
                 qPrintable(QStringLiteral("corner stem centre %1 is not centred under "
                                           "the right column centre %2 (off by %3 px)")
                                .arg(stemCentre2 / 2.0).arg(colCentre2 / 2.0)
                                .arg((stemCentre2 - colCentre2) / 2.0)));
    }

    // The editor frame margin must clear the border column, or the child
    // widgets (editor body, editor) start ON the column and paint over its
    // outer stroke - which is why the left border looked "thicker on one side"
    // at the bottom-left corner.
    //
    // Task 19: the inset lives on the CONTENT row, not on the host's layout.
    // The tab row above it spans the window edge to edge (its left control sits
    // in the window's first cell, above the ║ column), so a host-wide margin
    // would push the tab row off the window edge.
    void editor_frame_margin_clears_the_border_column()
    {
        ShellWindow w;
        w.resize(900, 400);
        w.show();
        QCoreApplication::processEvents();

        auto* host = w.findChild<QWidget*>(QStringLiteral("editorHost"));
        QVERIFY2(host, "editorHost not found");
        auto* body = w.findChild<QWidget*>(QStringLiteral("editorBody"));
        QVERIFY2(body && body->layout(), "editor body row not found");

        // One character cell, measured NOW (after the QSS font is applied).
        const int cellW = qMax(6, host->fontMetrics().horizontalAdvance(QLatin1Char('M')));
        const int margin = body->layout()->contentsMargins().left();
        QVERIFY2(margin == cellW,
                 qPrintable(QStringLiteral("editor frame margin is %1 px but the border "
                                           "column is %2 px wide - children start on the "
                                           "column and paint over its outer stroke")
                                .arg(margin).arg(cellW)));

        // The content row itself is inset inside the host by that same cell.
        QCOMPARE(body->mapTo(host, QPoint(0, 0)).x(), 0);   // full width, paints nothing
        QWidget* slot = w.findChild<QWidget*>(QStringLiteral("editorSlot"));
        QVERIFY2(slot, "editor slot not found");
        QVERIFY2(slot->mapTo(host, QPoint(0, 0)).x() >= cellW,
                 "editor slot starts on the border column");
    }

    // The left border's strokes must be identical above and inside the corner:
    // ║ (the column) and ╚ (the corner) are both double-line glyphs, so their
    // two vertical strokes must line up column for column.
    void left_border_strokes_match_the_bottom_left_corner()
    {
        ShellWindow w;
        w.resize(900, 400);
        w.show();
        QCoreApplication::processEvents();

        auto* host = w.findChild<QWidget*>(QStringLiteral("editorHost"));
        BottomBand* band = w.bottomBand();
        QVERIFY(host && band);

        const int cellW = qMax(6, band->fontMetrics().horizontalAdvance(QLatin1Char('M')));

        // Column strokes: sample a row in the middle of the editor area.
        const QImage hostImg = host->grab().toImage();
        QVERIFY2(!hostImg.isNull() && hostImg.height() > 40, "editorHost did not render");
        const int midY = hostImg.height() / 2;
        QString columnInk, cornerInk;
        for (int x = 0; x < cellW; ++x) {
            columnInk += isFrameWhite(hostImg.pixelColor(x, midY)) ? QLatin1Char('#')
                                                                  : QLatin1Char('.');
        }

        // Corner strokes: a row above the corner's horizontal bars.
        const QImage bandImg = band->grab().toImage();
        QVERIFY2(!bandImg.isNull() && bandImg.height() > 8, "bottom band did not render");
        for (int x = 0; x < cellW; ++x) {
            cornerInk += isFrameWhite(bandImg.pixelColor(x, 2)) ? QLatin1Char('#')
                                                                : QLatin1Char('.');
        }

        QVERIFY2(columnInk == cornerInk,
                 qPrintable(QStringLiteral("left border strokes differ: column [%1] vs "
                                           "corner [%2] - the corner is thicker on one side")
                                .arg(columnInk, cornerInk)));
    }

    // The document editor's TOP border row is drawn BELOW the tab row:
    //   ╔ in the upper-left cell, ═ running right, two spaces, the open file's
    //   name, two spaces, ═ again, then ╗ in the cell above the scrollbar.
    void title_row_has_corners_rule_and_the_file_name()
    {
        ShellWindow w;
        w.resize(900, 400);
        w.show();
        QCoreApplication::processEvents();

        auto* host = w.findChild<QWidget*>(QStringLiteral("editorHost"));
        QVERIFY2(host, "editorHost not found");
        auto* name = w.findChild<QLabel*>(QStringLiteral("editorTitle"));
        QVERIFY2(name, "editorTitle (the file name in the top border row) not found");
        QVERIFY2(!name->text().isEmpty(), "editorTitle has no text");

        const QImage img = host->grab().toImage();
        QVERIFY2(!img.isNull(), "editorHost did not render");
        const int cw = qMax(6, host->fontMetrics().horizontalAdvance(QLatin1Char('M')));
        const QRect row = name->geometry();
        QVERIFY2(row.height() >= 8 && row.top() + row.height() <= img.height(),
                 qPrintable(QStringLiteral("title row %1+%2 is not a sane row inside the host")
                                .arg(row.top()).arg(row.height())));

        const auto inkAt = [&](int x, int y) { return isFrameWhite(img.pixelColor(x, y)); };
        const auto inkIn = [&](const QRect& r) {
            int n = 0;
            for (int y = r.top(); y <= r.bottom(); ++y)
                for (int x = r.left(); x <= r.right(); ++x)
                    if (inkAt(x, y))
                        ++n;
            return n;
        };

        // (1) ╔: the cell carries the two horizontal strokes of a CORNER. The
        // tell is that ink reaches the cell's LAST column on those rows - a ║
        // never inks beyond its two stroke columns (measured: ║ is `..##.##.`).
        const QRect leftCell(0, row.top(), cw, row.height());
        int cornerRows = 0;
        for (int y = leftCell.top(); y <= leftCell.bottom(); ++y)
            if (inkAt(leftCell.right(), y))
                ++cornerRows;
        QVERIFY2(cornerRows >= 2,
                 qPrintable(QStringLiteral("upper-left cell is not a ╔ corner: only %1 row(s) "
                                           "reach the cell's last column (a ║ reaches none)")
                                .arg(cornerRows)));

        // (2) ╗: the last cell is a corner too, and it sits in the SAME cell as
        // the scrollbar column, so the row terminates on the bar's top edge.
        auto* bar = w.findChild<RightScrollBar*>(QStringLiteral("rightScrollBar"));
        QVERIFY2(bar, "rightScrollBar not found");
        const int barLeft = bar->mapTo(host, QPoint(0, 0)).x();
        QVERIFY2(barLeft == img.width() - cw,
                 qPrintable(QStringLiteral("scrollbar column starts at x=%1 but the title row's "
                                           "last cell starts at x=%2 - ╗ cannot align with it")
                                .arg(barLeft).arg(img.width() - cw)));
        const QRect rightCell(img.width() - cw, row.top(), cw, row.height());
        int rightCornerRows = 0;
        for (int y = rightCell.top(); y <= rightCell.bottom(); ++y)
            if (inkAt(rightCell.left(), y))
                ++rightCornerRows;
        QVERIFY2(rightCornerRows >= 2,
                 qPrintable(QStringLiteral("last cell of the title row is not a ╗ corner "
                                           "(%1 row(s) reach its first column)")
                                .arg(rightCornerRows)));

        // (3) The name is bracketed by exactly TWO character cells of daylight.
        const QRect twoLeft(row.left() - 2 * cw, row.top(), 2 * cw, row.height());
        const QRect twoRight(row.right() + 1, row.top(), 2 * cw, row.height());
        QVERIFY2(inkIn(twoLeft) == 0,
                 qPrintable(QStringLiteral("the two cells before the file name are not clear "
                                           "(%1 white px)").arg(inkIn(twoLeft))));
        QVERIFY2(inkIn(twoRight) == 0,
                 qPrintable(QStringLiteral("the two cells after the file name are not clear "
                                           "(%1 white px)").arg(inkIn(twoRight))));

        // (4) ═ runs from the ╔ to the name's gap and on to the ╗.
        const QRect leftRun(cw, row.top(), row.left() - 3 * cw, row.height());
        const QRect rightRun(twoRight.right() + 1, row.top(),
                             img.width() - cw - (twoRight.right() + 1), row.height());
        QVERIFY2(inkIn(leftRun) > 2 * cw,
                 qPrintable(QStringLiteral("no ═ rule between ╔ and the file name (%1 px)")
                                .arg(inkIn(leftRun))));
        QVERIFY2(inkIn(rightRun) > 2 * cw,
                 qPrintable(QStringLiteral("no ═ rule between the file name and ╗ (%1 px)")
                                .arg(inkIn(rightRun))));
    }

    // ╔ must line up with the ║ column beneath it: same two stroke columns, so
    // the join reads as one continuous double line.
    void title_corner_dovetails_with_the_left_border_column()
    {
        ShellWindow w;
        w.resize(900, 400);
        w.show();
        QCoreApplication::processEvents();

        auto* host = w.findChild<QWidget*>(QStringLiteral("editorHost"));
        auto* name = w.findChild<QLabel*>(QStringLiteral("editorTitle"));
        QVERIFY(host && name);
        const QImage img = host->grab().toImage();
        QVERIFY(!img.isNull());
        const int cw = qMax(6, host->fontMetrics().horizontalAdvance(QLatin1Char('M')));
        const QRect row = name->geometry();
        const int midY = row.bottom() + 1 + img.height() / 3;
        QVERIFY2(midY < img.height(), "no room below the title row to sample the column");

        // Sample the BOTTOM row of the corner cell: by then the ╔'s horizontal
        // strokes are behind it and only its two vertical strokes remain, so it
        // must match the ║ cell column for column.
        QString corner, column;
        for (int x = 0; x < cw; ++x) {
            corner += isFrameWhite(img.pixelColor(x, row.bottom())) ? QLatin1Char('#')
                                                                    : QLatin1Char('.');
            column += isFrameWhite(img.pixelColor(x, midY)) ? QLatin1Char('#')
                                                            : QLatin1Char('.');
        }
        QVERIFY2(corner == column,
                 qPrintable(QStringLiteral("title corner [%1] does not line up with the ║ "
                                           "column [%2] - the join is stepped")
                                .arg(corner, column)));
    }

    // A tiled rule must stop AT its band: the tile helper draws whole
    // characters, so a band that is not a whole number of cells wide used to
    // overdraw up to one cell past its edge and bleed into the corner glyph it
    // terminates beside.
    void tiled_rule_does_not_overdraw_its_band()
    {
        const int cw = 8;
        QImage img(cw * 6, 16, QImage::Format_RGB32);
        img.fill(QColor(0x00, 0x00, 0xAA));
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing, false);
        QFont f(QStringLiteral("PxPlus IBM VGA8"));
        f.setPixelSize(16);
        p.setFont(f);
        // A band 3 cells + 4 px wide: the fourth tile must be clipped at 28.
        const QRect band(0, 0, cw * 3 + 4, 16);
        TvFrame::tileH(p, band, cw, QChar(0x2550), QColor(0xFF, 0xFF, 0xFF));
        p.end();

        int outside = 0;
        for (int y = 0; y < img.height(); ++y)
            for (int x = band.right() + 1; x < img.width(); ++x)
                if (isFrameWhite(img.pixelColor(x, y)))
                    ++outside;
        QVERIFY2(outside == 0,
                 qPrintable(QStringLiteral("%1 white px painted past the band's right edge "
                                           "(x>%2) - the rule bleeds into the corner cell")
                                .arg(outside).arg(band.right())));
    }
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    TstFrame tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "tst_frame.moc"
