// SPDX-License-Identifier: GPL-3.0-or-later
// Task 12: Turbo Vision title-line tab strip contract test.
// Proves: (1) the strip paints a green [■] close box and a green [↕] zoom box;
// (2) tabs render as title-line segments with the active tab highlighted;
// (3) clicking a tab emits tabActivated(index).
#include <QtTest>
#include <QImage>
#include <QPainter>
#include <QMouseEvent>
#include <climits>
#include "src/shell/TabStrip.h"
#include "src/shell/ShellWindow.h"

namespace {
const QColor kGreen(0x00, 0xAA, 0x00);
const QColor kBlue(0x00, 0x00, 0xAA);
const QColor kBandGrey(0xAA, 0xAA, 0xAA);   // stipple ink of the tab row (bright grey)
const QColor kWhite(0xFF, 0xFF, 0xFF);
const QColor kTabGrey(0x88, 0x88, 0x88);    // ink of an inactive chip (on the editor blue)
const QColor kTeal(0x00, 0xAA, 0xAA);       // control-cell fill

int cellWOf(const QWidget& w)
{
    return qMax(6, w.fontMetrics().horizontalAdvance(QLatin1Char('M')));
}

int countColor(const QImage& img, const QColor& t, int tol)
{
    int n = 0;
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x) {
            const QColor c = img.pixelColor(x, y);
            if (qAbs(c.red() - t.red()) <= tol && qAbs(c.green() - t.green()) <= tol &&
                qAbs(c.blue() - t.blue()) <= tol)
                ++n;
        }
    return n;
}

bool isNear(const QColor& c, const QColor& t, int tol = 24)
{
    return qAbs(c.red() - t.red()) <= tol && qAbs(c.green() - t.green()) <= tol &&
           qAbs(c.blue() - t.blue()) <= tol;
}
} // namespace

class TstTabStrip : public QObject
{
    Q_OBJECT
private slots:
    // Task 19: the tab row spans the WHOLE window width (it starts at the
    // window's left edge, above the frame's left column, exactly like the
    // mock's left arrow cell) and is exactly ONE character cell tall - the
    // same row height as the document border row below it.
    void strip_is_full_width_and_one_cell_tall() {
        ShellWindow w;
        w.resize(900, 600);
        TabStrip* ts = w.tabStrip();
        QVERIFY2(ts, "no tab strip");
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        QCoreApplication::processEvents();

        QCOMPARE(ts->mapTo(&w, QPoint(0, 0)).x(), 0);
        QCOMPARE(ts->width(), w.centralWidget()->width());
        QVERIFY2(ts->height() == ts->fontMetrics().height(),
                 qPrintable(QStringLiteral("strip height=%1, one cell=%2")
                                .arg(ts->height()).arg(ts->fontMetrics().height())));
    }

    // Task 19 (mock note 2): the row's background is the scrollbar track's
    // stipple design with GREY ink instead of teal, and the row carries NO
    // rule of its own - the document's top border row below it is the only
    // border rule.
    void band_is_grey_dither_on_blue_with_no_rule() {
        TabStrip ts;
        ts.setTabs(QStringList() << QStringLiteral("a"), 0);
        const int cellH = qMax(8, ts.fontMetrics().height());
        ts.resize(800, cellH);
        const QImage img = ts.grab().toImage();
        QVERIFY(!img.isNull());

        // Both inks are present, in roughly equal share: a 1-px checkerboard.
        const int blue = countColor(img, kBlue, 8);
        const int grey = countColor(img, kBandGrey, 8);
        const int total = img.width() * img.height();
        QVERIFY2(blue * 100 / total >= 30 && blue * 100 / total <= 65,
                 qPrintable(QStringLiteral("band blue share=%1%").arg(blue * 100 / total)));
        QVERIFY2(grey * 100 / total >= 30 && grey * 100 / total <= 65,
                 qPrintable(QStringLiteral("band grey share=%1%").arg(grey * 100 / total)));

        // No rule: no row may be mostly white. The old title line ran a white
        // double rule the full width (measured 1150/1280 px per row).
        for (int y = 0; y < img.height(); ++y) {
            int white = 0;
            for (int x = 0; x < img.width(); ++x)
                if (isNear(img.pixelColor(x, y), kWhite))
                    ++white;
            QVERIFY2(white * 2 <= img.width(),
                     qPrintable(QStringLiteral("row %1 is %2/%3 white - a rule is still drawn")
                                    .arg(y).arg(white).arg(img.width())));
        }

        // Dither, not stripes or a solid fill: in an empty stretch of band (no
        // chip, no control) no two ADJACENT pixels share the grey ink, in
        // EITHER axis.
        //
        // Both axes are required. A single-axis check passes a STRIPE pattern:
        // with `y & 1` the sampled row 0 happens to be the blue row, and the
        // column sample alternates - so the test was green while the row was
        // banded. (Found by mutation-testing this very assertion.)
        const int x0 = img.width() / 2;
        const int span = 40;
        for (int y = 0; y < img.height(); ++y)
            for (int x = x0; x + 1 < x0 + span && x + 1 < img.width(); ++x)
                QVERIFY2(!(isNear(img.pixelColor(x, y), kBandGrey, 8) &&
                           isNear(img.pixelColor(x + 1, y), kBandGrey, 8)),
                         qPrintable(QStringLiteral("horizontal grey run at (%1,%2) - "
                                                   "the band is striped, not dithered")
                                        .arg(x).arg(y)));
        for (int x = x0; x < x0 + span && x < img.width(); ++x)
            for (int y = 0; y + 1 < img.height(); ++y)
                QVERIFY2(!(isNear(img.pixelColor(x, y), kBandGrey, 8) &&
                           isNear(img.pixelColor(x, y + 1), kBandGrey, 8)),
                         qPrintable(QStringLiteral("vertical grey run at (%1,%2) - "
                                                   "the band is striped, not dithered")
                                        .arg(x).arg(y)));
    }

    // Task 20 (mock): the row's four controls sit in CELLS at the row's ends -
    // ◄ in the window's first cell, and ► + ▼ packed against the right edge
    // with ▼ in the LAST cell, i.e. directly above the right scrollbar column.
    void controls_sit_at_the_row_ends_in_cell_units() {
        TabStrip ts;
        ts.setTabs(QStringList() << "alpha", 0);
        const int cellW = qMax(6, ts.fontMetrics().horizontalAdvance(QLatin1Char('M')));
        const int cellH = qMax(8, ts.fontMetrics().height());
        ts.resize(600, cellH);

        const QRect prev = ts.prevTabRect(), next = ts.nextTabRect();
        const QRect add = ts.newTabRect(), list = ts.tabListRect();
        QCOMPARE(prev.left(), 0);
        // Task 25: each control is a scrollbar-sized square = two cells wide.
        QCOMPARE(prev.width(), 2 * cellW);
        QCOMPARE(list.right(), ts.width() - 1);          // last cell of the row
        QCOMPARE(list.width(), 2 * cellW);
        QCOMPARE(add.right(), list.left() - 1);          // + immediately left of ▼
        QCOMPARE(next.right(), add.left() - 1);          // ► immediately left of +
        QCOMPARE(prev.height(), cellH);

        // A cell of the strip's background sits between ◄ and the first chip,
        // and the chips never run into ►.
        const QVector<QRect> rects = ts.tabRects();
        QCOMPARE(rects.size(), 1);
        QCOMPARE(rects[0].left(), prev.right() + 1 + cellW);
        QVERIFY(rects[0].right() < next.left());
    }

    // Task 20 (mock notes 2 + 3): each tab is its own bracketed chip with a
    // BRIGHT green square, in BOTH states; the active title is white and the
    // inactive title is grey.
    void chips_are_bracketed_with_a_bright_green_square() {
        TabStrip ts;
        ts.setTabs(QStringList() << "active" << "idle", 0);
        const int cellH = qMax(8, ts.fontMetrics().height());
        ts.resize(600, cellH);
        const QImage img = ts.grab().toImage();
        QVERIFY(!img.isNull());
        const QVector<QRect> rects = ts.tabRects();
        QCOMPARE(rects.size(), 2);

        auto countIn = [&](const QRect& r, const QColor& t, int tol) {
            int n = 0;
            for (int y = qMax(0, r.top()); y <= qMin(img.height() - 1, r.bottom()); ++y)
                for (int x = qMax(0, r.left()); x <= qMin(img.width() - 1, r.right()); ++x) {
                    const QColor c = img.pixelColor(x, y);
                    if (qAbs(c.red() - t.red()) <= tol && qAbs(c.green() - t.green()) <= tol &&
                        qAbs(c.blue() - t.blue()) <= tol)
                        ++n;
                }
            return n;
        };

        // The green square is bright in BOTH chips (mock note 3) and lives
        // inside the chip, left of the closing bracket.
        for (int i = 0; i < 2; ++i) {
            const QRect sq = ts.closeRect(i);
            QVERIFY2(sq.width() > 0 && rects[i].contains(sq),
                     qPrintable(QStringLiteral("chip %1 has no close square").arg(i)));
            const int green = countIn(sq, kGreen, 8);
            QVERIFY2(green >= 6, qPrintable(QStringLiteral("chip %1 close square green px=%2")
                                                .arg(i).arg(green)));
            // The brackets are ink of the chip's own state, not green.
            QVERIFY2(countIn(QRect(rects[i].left(), 0, cellWOf(ts), cellH), kGreen, 8) == 0,
                     "opening bracket is green - it must carry the chip's ink");
        }

        // Ink per state, measured on the name cell band (square excluded).
        auto nameBand = [&](int i) {
            const QRect r = rects[i];
            const QRect sq = ts.closeRect(i);
            return QRect(r.left() + cellWOf(ts), r.top(), sq.left() - r.left() - cellWOf(ts),
                         r.height());
        };
        QVERIFY2(countIn(nameBand(0), kWhite, 8) >= 3, "active chip title is not white");
        QVERIFY2(countIn(nameBand(1), kTabGrey, 8) >= 3, "inactive chip title is not grey");
        // The two states are distinguished by GREY density: the band itself is
        // now bright grey (#AAAAAA), so an absolute "no grey" cannot work - the
        // inactive chip must still carry visibly more grey than the active one,
        // whose only grey is the dither showing through.
        QVERIFY2(countIn(nameBand(1), kTabGrey, 8) >= countIn(nameBand(0), kTabGrey, 8) + 40,
                 qPrintable(QStringLiteral("chip states not distinguished: active grey=%1, "
                                           "inactive grey=%2")
                                .arg(countIn(nameBand(0), kTabGrey, 8))
                                .arg(countIn(nameBand(1), kTabGrey, 8))));
        QVERIFY2(countIn(nameBand(1), kWhite, 8) == 0, "inactive chip is drawn bright");
    }

    void tabs_render_as_title_line_segments() {
        TabStrip ts;
        ts.setTabs(QStringList() << "alpha" << "beta" << "gamma", 1);
        const QVector<QRect> rects = ts.tabRects();
        QCOMPARE(rects.size(), 3);
        // Active tab (index 1) is the highlighted (filled) one; assert segments
        // are ordered left-to-right and non-overlapping.
        for (int i = 1; i < rects.size(); ++i)
            QVERIFY2(rects[i].left() > rects[i - 1].right(),
                     "tab segments overlap or are out of order");
        QCOMPARE(ts.activeIndex(), 1);
        QCOMPARE(ts.titles().size(), 3);
    }

    // Task 20 (mock): the green square inside a chip IS that tab's close
    // button, and it must not also activate the tab.
    void clicking_the_close_square_emits_tabClosed() {
        TabStrip ts;
        ts.setTabs(QStringList() << "one" << "two", 0);
        const int cellH = qMax(8, ts.fontMetrics().height());
        ts.resize(600, cellH);
        ts.show();
        QVERIFY(QTest::qWaitForWindowExposed(&ts));
        ts.grab();
        QCoreApplication::processEvents();
        QVERIFY2(ts.tabRects().size() == 2, "chip rects not computed after render");

        QSignalSpy closed(&ts, &TabStrip::tabClosed);
        QSignalSpy activated(&ts, &TabStrip::tabActivated);
        QTest::mouseClick(&ts, Qt::LeftButton, Qt::NoModifier, ts.closeRect(1).center());
        QCOMPARE(closed.count(), 1);
        QCOMPARE(closed.at(0).at(0).toInt(), 1);
        QCOMPARE(activated.count(), 0);
    }

    // Task 20 (mock notes 4 + 5): + asks for a new document, ▼ asks for the
    // tab list. Both are persistent.
    void clicking_new_tab_and_tab_list_emit_their_signals() {
        TabStrip ts;
        ts.setTabs(QStringList() << "one", 0);
        const int cellH = qMax(8, ts.fontMetrics().height());
        ts.resize(600, cellH);
        ts.show();
        QVERIFY(QTest::qWaitForWindowExposed(&ts));

        QSignalSpy add(&ts, &TabStrip::newTabRequested);
        QSignalSpy list(&ts, &TabStrip::tabListRequested);
        QTest::mouseClick(&ts, Qt::LeftButton, Qt::NoModifier, ts.newTabRect().center());
        QCOMPARE(add.count(), 1);
        QCOMPARE(list.count(), 0);
        QTest::mouseClick(&ts, Qt::LeftButton, Qt::NoModifier, ts.tabListRect().center());
        QCOMPARE(list.count(), 1);
        QCOMPARE(add.count(), 1);
    }

    // Task 20 (mock note 1): ◄ / ► scroll the chip window one chip at a time
    // and are present even when nothing overflows.
    // Task 25 (user): the four controls must be easier hit targets - the size
    // of the horizontal scrollbar's end buttons. Those are SQUARES whose width
    // is the bar's thickness, i.e. two cells here (the row is one cell tall),
    // so each control is 2 cells wide and the right-hand three are adjacent.
    void controls_are_scrollbar_sized_hit_targets() {
        TabStrip ts;
        ts.setTabs(QStringList() << "alpha", 0);
        const int cellH = qMax(8, ts.fontMetrics().height());
        ts.resize(800, cellH);
        const int cw = cellWOf(ts);
        const QRect prev = ts.prevTabRect();
        const QRect next = ts.nextTabRect();
        const QRect nw = ts.newTabRect();
        const QRect list = ts.tabListRect();
        const QRect all[] = { prev, next, nw, list };
        const char* names[] = { "◄", "►", "+", "▼" };
        for (int i = 0; i < 4; ++i)
            QVERIFY2(all[i].width() == 2 * cw,
                     qPrintable(QStringLiteral("%1 control is %2 px wide, want two cells (%3)")
                                    .arg(names[i]).arg(all[i].width()).arg(2 * cw)));
        QCOMPARE(prev.left(), 0);
        QCOMPARE(list.right(), ts.width() - 1);
        QCOMPARE(next.left(), nw.left() - 2 * cw);   // adjacent pair
        QCOMPARE(nw.left(), list.left() - 2 * cw);
    }

    // Task 25 (user): the strip's own background shows between the first chip
    // and ◄, and between the last chip and ►. The trailing gap is checked across
    // a SWEEP of widths: at any single width the chips may happen to stop short
    // of the ► control whatever the limit is, so only a sweep proves the limit
    // itself reserves the cell.
    void chips_are_inset_from_the_controls() {
        TabStrip ts;
        QStringList many;
        for (int i = 0; i < 40; ++i)
            many << QStringLiteral("t%1").arg(i);
        ts.setTabs(many, 0);
        const int cellH = qMax(8, ts.fontMetrics().height());
        const int cw = cellWOf(ts);

        int worstTrail = 1 << 30, worstW = 0;
        for (int w = 240; w <= 900; ++w) {
            ts.resize(w, cellH);
            const QVector<QRect> rects = ts.tabRects();
            if (rects.isEmpty())
                continue;
            const int trail = ts.nextTabRect().left() - rects.last().right() - 1;
            if (trail < worstTrail) {
                worstTrail = trail;
                worstW = w;
            }
        }
        QVERIFY2(worstTrail >= cw,
                 qPrintable(QStringLiteral("only %1 px between the last chip and ► at "
                                           "width %2 (want a whole cell, %3)")
                                .arg(worstTrail).arg(worstW).arg(cw)));

        // The leading gap, at the same sweep.
        int worstLead = 1 << 30;
        for (int w = 240; w <= 900; w += 13) {
            ts.resize(w, cellH);
            const QVector<QRect> rects = ts.tabRects();
            if (rects.isEmpty())
                continue;
            worstLead = qMin(worstLead, rects.first().left() - ts.prevTabRect().right() - 1);
        }
        QVERIFY2(worstLead >= cw,
                 qPrintable(QStringLiteral("only %1 px between ◄ and the first chip")
                                .arg(worstLead)));
    }

    // Task 22 (mock notes 1, 4, 5): the controls are drawn the way the
    // scrollbars draw their end cells - a SOLID CYAN cell with a DARK BLUE
    // glyph in it - not a teal glyph floating on the grey dither.
    void controls_are_blue_glyphs_on_solid_cyan_cells() {
        TabStrip ts;
        ts.setTabs(QStringList() << "alpha", 0);
        const int cellH = qMax(8, ts.fontMetrics().height());
        ts.resize(600, cellH);
        const QImage img = ts.grab().toImage();
        QVERIFY(!img.isNull());

        auto counts = [&](const QRect& cell) {
            int teal = 0, blue = 0, band = 0;
            for (int y = qMax(0, cell.top()); y <= qMin(img.height() - 1, cell.bottom()); ++y)
                for (int x = qMax(0, cell.left()); x <= qMin(img.width() - 1, cell.right()); ++x) {
                    const QColor c = img.pixelColor(x, y);
                    if (isNear(c, kTeal))
                        ++teal;
                    else if (isNear(c, kBandGrey, 6))
                        ++band;
                    else if (isNear(c, kBlue, 6))
                        ++blue;
                }
            return QString("teal=%1 blue=%2 band=%3").arg(teal).arg(blue).arg(band);
        };

        const QRect cells[] = { ts.prevTabRect(), ts.nextTabRect(),
                                ts.newTabRect(), ts.tabListRect() };
        const char* names[] = { "◄", "►", "+", "▼" };
        for (int i = 0; i < 4; ++i) {
            int teal = 0, blue = 0, band = 0;
            for (int y = qMax(0, cells[i].top()); y <= qMin(img.height() - 1, cells[i].bottom()); ++y)
                for (int x = qMax(0, cells[i].left()); x <= qMin(img.width() - 1, cells[i].right()); ++x) {
                    const QColor c = img.pixelColor(x, y);
                    if (isNear(c, kTeal))
                        ++teal;
                    else if (isNear(c, kBandGrey, 6))
                        ++band;
                    else if (isNear(c, kBlue, 6))
                        ++blue;
                }
            Q_UNUSED(counts);
            // The cell is FILLED: the dither underneath must not show through.
            QVERIFY2(teal * 2 >= (cells[i].width() * cells[i].height()),
                     qPrintable(QStringLiteral("%1 control is not a solid cyan cell (%2)")
                                    .arg(names[i])
                                    .arg(QString("%1 teal, %2 blue, %3 band").arg(teal).arg(blue).arg(band))));
            // ... and it carries a DARK BLUE glyph on top of the cyan.
            QVERIFY2(blue >= 4,
                     qPrintable(QStringLiteral("%1 control has no blue glyph (%2 teal, %3 blue)")
                                    .arg(names[i]).arg(teal).arg(blue)));
        }
    }

    // Task 22 (user): a cell of daylight between chips, as the mock shows -
    // the chips were touching (`][`) and read as one run.
    // Task 23 (user): a chip is a window onto the editor - everything from `[`
    // to `]` sits on the EDITOR's background, not on the strip's brighter
    // dither, so each tab stands out from the band. Active AND inactive.
    void chip_interiors_are_the_editor_background() {
        TabStrip ts;
        ts.setTabs(QStringList() << "alpha" << "beta", 0);
        const int cellH = qMax(8, ts.fontMetrics().height());
        ts.resize(800, cellH);
        const QImage img = ts.grab().toImage();
        QVERIFY(!img.isNull());
        const QVector<QRect> rects = ts.tabRects();
        QCOMPARE(rects.size(), 2);

        for (int i = 0; i < rects.size(); ++i) {
            const QRect r = rects.at(i);
            int blue = 0, band = 0;
            for (int y = qMax(0, r.top()); y <= qMin(img.height() - 1, r.bottom()); ++y)
                for (int x = qMax(0, r.left()); x <= qMin(img.width() - 1, r.right()); ++x) {
                    const QColor c = img.pixelColor(x, y);
                    if (isNear(c, kBlue, 6))
                        ++blue;
                    else if (isNear(c, kBandGrey, 6))
                        ++band;
                }
            // Not one pixel of the strip's dither may show inside a chip.
            QVERIFY2(band == 0,
                     qPrintable(QStringLiteral("chip %1: %2 strip-dither px inside the "
                                               "brackets - the band bleeds through")
                                    .arg(i).arg(band)));
            // ... and the editor blue has to be what fills it.
            QVERIFY2(blue * 2 >= r.width() * r.height(),
                     qPrintable(QStringLiteral("chip %1 is not on the editor background "
                                               "(%2 blue of %3 px)")
                                    .arg(i).arg(blue).arg(r.width() * r.height())));
        }
    }

    void chips_are_separated_by_daylight() {
        TabStrip ts;
        ts.setTabs(QStringList() << "alpha" << "beta" << "gamma", 0);
        const int cellH = qMax(8, ts.fontMetrics().height());
        const int cw = cellWOf(ts);
        ts.resize(800, cellH);
        const QVector<QRect> rects = ts.tabRects();
        QCOMPARE(rects.size(), 3);
        for (int i = 1; i < rects.size(); ++i)
            QVERIFY2(rects[i].left() - rects[i - 1].right() - 1 >= cw,
                     qPrintable(QStringLiteral("chips %1/%2 are %3 px apart, need a whole cell")
                                    .arg(i - 1).arg(i)
                                    .arg(rects[i].left() - rects[i - 1].right() - 1)));
    }

    void arrows_scroll_the_chip_window() {
        TabStrip ts;
        ts.setTabs(QStringList() << "document1" << "document2" << "document3"
                                 << "document4" << "document5" << "document6", 0);
        const int cellH = qMax(8, ts.fontMetrics().height());
        ts.resize(600, cellH);
        ts.show();
        QVERIFY(QTest::qWaitForWindowExposed(&ts));
        ts.grab();
        QCoreApplication::processEvents();

        QVERIFY2(ts.tabRects().size() < 6,
                 qPrintable(QStringLiteral("expected overflow, got %1 chips").arg(ts.tabRects().size())));
        QCOMPARE(ts.firstVisibleTab(), 0);
        QVERIFY2(!ts.closeRect(0).isEmpty(), "tab 0 not visible at the start");

        QTest::mouseClick(&ts, Qt::LeftButton, Qt::NoModifier, ts.nextTabRect().center());
        QCOMPARE(ts.firstVisibleTab(), 1);
        // The window really moved: tab 0 is gone from the row, tab 1 is on it.
        QVERIFY2(ts.closeRect(0).isEmpty(), "tab 0 is still visible after scrolling right");
        QVERIFY2(!ts.closeRect(1).isEmpty(), "tab 1 is not visible after scrolling right");

        QTest::mouseClick(&ts, Qt::LeftButton, Qt::NoModifier, ts.prevTabRect().center());
        QCOMPARE(ts.firstVisibleTab(), 0);

        // Clamped at the ends: ◄ at the start is a no-op, not a wrap-around.
        QTest::mouseClick(&ts, Qt::LeftButton, Qt::NoModifier, ts.prevTabRect().center());
        QCOMPARE(ts.firstVisibleTab(), 0);
    }

    void clicking_a_tab_emits_tabActivated() {
        TabStrip ts;
        ts.setTabs(QStringList() << "one" << "two" << "three", 0);
        ts.resize(600, 26);
        ts.show();
        QVERIFY(QTest::qWaitForWindowExposed(&ts));
        const QVector<QRect> rects = ts.tabRects();
        QVERIFY(rects.size() == 3);

        QSignalSpy spy(&ts, &TabStrip::tabActivated);
        // Click the 3rd tab's center (public path: a real mouse event).
        QTest::mouseClick(&ts, Qt::LeftButton, Qt::NoModifier, rects[2].center());
        QVERIFY2(spy.count() == 1,
                 qPrintable(QStringLiteral("tabActivated count=%1 after clicking tab 2").arg(spy.count())));
        QCOMPARE(spy.at(0).at(0).toInt(), 2);

        // The composition root responds by switching the doc and re-syncing the
        // strip (setTabs). Now tab 2 is active, so re-clicking it emits nothing.
        ts.setTabs(QStringList() << "one" << "two" << "three", 2);
        QTest::mouseClick(&ts, Qt::LeftButton, Qt::NoModifier, rects[2].center());
        QCOMPARE(spy.count(), 1);
    }

    // The green close square must be a SQUARE. It is drawn as
    // closeRect(idx).adjusted(2, 3, -2, -3); on a one-cell-tall row that is
    // (cellW-4) wide by (cellH-6) tall - e.g. 6x10 at an 8x16 cell, a tall
    // rectangle. The green INK bounding box must be square (width == height).
    void close_square_is_square() {
        TabStrip ts;
        ts.setTabs(QStringList() << "active" << "idle", 0);
        const int cellH = qMax(8, ts.fontMetrics().height());
        ts.resize(600, cellH);
        const QImage img = ts.grab().toImage();
        QVERIFY(!img.isNull());
        const QVector<QRect> rects = ts.tabRects();
        QCOMPARE(rects.size(), 2);

        for (int i = 0; i < 2; ++i) {
            const QRect sq = ts.closeRect(i);
            int minX = INT_MAX, maxX = INT_MIN, minY = INT_MAX, maxY = INT_MIN;
            for (int y = qMax(0, sq.top()); y <= qMin(img.height() - 1, sq.bottom()); ++y)
                for (int x = qMax(0, sq.left()); x <= qMin(img.width() - 1, sq.right()); ++x) {
                    const QColor c = img.pixelColor(x, y);
                    if (isNear(c, kGreen, 8)) {
                        minX = qMin(minX, x); maxX = qMax(maxX, x);
                        minY = qMin(minY, y); maxY = qMax(maxY, y);
                    }
                }
            QVERIFY2(minX <= maxX, qPrintable(QStringLiteral("chip %1 has no green ink").arg(i)));
            const int inkW = maxX - minX + 1, inkH = maxY - minY + 1;
            QVERIFY2(inkW == inkH,
                     qPrintable(QStringLiteral("chip %1 close square is %2x%3 (want square)")
                                    .arg(i).arg(inkW).arg(inkH)));
        }
    }

    // The + (new-tab) glyph must be as prominent as the arrows flanking it.
    // It is currently drawn with TvFrame::glyph(), which renders the font's own
    // small '+' - a few pixels of blue in a two-cell control, dwarfed by the
    // hand-built arrow polygons. The blue INK must fill a substantial fraction
    // of the control cell (>= 30% of its area).
    void new_tab_plus_glyph_is_large() {
        TabStrip ts;
        ts.setTabs(QStringList() << "alpha", 0);
        const int cellH = qMax(8, ts.fontMetrics().height());
        ts.resize(600, cellH);
        const QImage img = ts.grab().toImage();
        QVERIFY(!img.isNull());

        const QRect add = ts.newTabRect();
        int blue = 0;
        for (int y = add.top(); y <= add.bottom(); ++y)
            for (int x = add.left(); x <= add.right(); ++x)
                if (isNear(img.pixelColor(x, y), kBlue, 8))
                    ++blue;
        const double frac = double(blue) / double(add.width() * add.height());
        QVERIFY2(frac >= 0.30,
                 qPrintable(QStringLiteral("+ glyph covers %1% of the control (want >=30%)")
                                .arg(frac * 100.0, 0, 'f', 1)));
    }

    // The arrows flanking + are the reference for "large": the + must be at
    // least as tall as the arrow ink it sits beside, so it does not read as a
    // tiny mark wedged between two solid triangles.
    void plus_is_as_tall_as_the_arrows() {
        TabStrip ts;
        ts.setTabs(QStringList() << "alpha", 0);
        const int cellH = qMax(8, ts.fontMetrics().height());
        ts.resize(600, cellH);
        const QImage img = ts.grab().toImage();
        QVERIFY(!img.isNull());

        auto inkHeight = [&](const QRect& r) {
            int minY = INT_MAX, maxY = INT_MIN;
            for (int y = r.top(); y <= r.bottom(); ++y)
                for (int x = r.left(); x <= r.right(); ++x)
                    if (isNear(img.pixelColor(x, y), kBlue, 8)) {
                        minY = qMin(minY, y); maxY = qMax(maxY, y);
                    }
            return (minY <= maxY) ? maxY - minY + 1 : 0;
        };
        const int addInk = inkHeight(ts.newTabRect());
        const int nextInk = inkHeight(ts.nextTabRect());
        QVERIFY2(nextInk > 0 && addInk > 0, "arrow or + has no blue ink");
        QVERIFY2(addInk >= nextInk,
                 qPrintable(QStringLiteral("+ ink height %1 < arrow height %2")
                                .arg(addInk).arg(nextInk)));
    }
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    TstTabStrip tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "tst_tabstrip.moc"
