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
    // Task 4: the DOS double-line frame box has all FOUR corners — the title
    // line (TabStrip) supplies the top two, the editor host the bottom two.
    // Sample the actual frame-box corners (not the menubar/status bar, which
    // are also gray and would mask a missing frame corner).
    void frame_has_all_four_corners() {
        ShellWindow w;
        w.resize(400, 300);
        w.show();
        QCoreApplication::processEvents();
        TabStrip* ts = w.tabStrip();
        BottomBand* band = w.bottomBand();
        QVERIFY(ts && band);
        const QImage img = w.grab().toImage();
        QVERIFY(!img.isNull());

        const QPoint tsOrigin = ts->mapTo(&w, QPoint(0, 0));
        const QPoint bOrigin = band->mapTo(&w, QPoint(0, 0));

        // Frame-box corners in shell coordinates:
        //   top-left/top-right: the title line's left/right ends.
        //   bottom-left/bottom-right: the BOTTOM BAND's BORDER ROW — the lower
        //   of the band's two rows (the scrollbar owns the upper one). The
        //   bottom-right corner is GREEN by spec, so it is matched separately.
        const int borderY = band->height() - band->fontMetrics().height() / 2;
        const QPoint corners[4] = {
            tsOrigin + QPoint(3, ts->height() - 4),                 // top-left
            tsOrigin + QPoint(ts->width() - 4, ts->height() - 4),   // top-right
            bOrigin + QPoint(3, borderY),                           // bottom-left
            bOrigin + QPoint(band->width() - 4, borderY)            // bottom-right
        };
        const char* names[4] = { "top-left", "top-right", "bottom-left", "bottom-right" };

        for (int i = 0; i < 4; ++i) {
            bool found = false;
            const QPoint c = corners[i];
            for (int dy = -3; dy <= 3 && !found; ++dy)
                for (int dx = -3; dx <= 3 && !found; ++dx) {
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
    void bottom_border_breaks_around_the_cursor_position()
    {
        ShellWindow w;
        w.resize(600, 400);
        w.show();
        QCoreApplication::processEvents();
        BottomBand* band = w.bottomBand();
        QVERIFY(band);
        // Grab the band itself: the shell is never mapped, so a shell grab plus
        // mapTo() is not a reliable way to address the band's own rows.
        const QImage img = band->grab().toImage();
        QVERIFY2(!img.isNull() && img.width() > 100, "bottom band did not render");
        const QPoint o(0, 0);
        const int bw = img.width(), bh = img.height();

        int upper = -1, lower = -1;
        QVERIFY2(ruleRows(img, o, bw, bh, &upper, &lower),
                 "no bottom rule row found in the band");

        // Between the left corner and the middle of the band the rule must be
        // INTERRUPTED: a space, "L:C", a space. A solid rule has no blue pixel
        // on that row.
        bool gap = false;
        for (int x = 40; x < bw / 2 && !gap; ++x)
            if (isBgBlue(img.pixelColor(o.x() + x, o.y() + upper)))
                gap = true;
        QVERIFY2(gap, "bottom rule is unbroken - the cursor position must interrupt it");
    }

    // The bottom-left corner is ╚: the ║ column from above stops AT the rule.
    // A full-height vertical draws a cross instead.
    void bottom_left_corner_is_a_corner_not_a_cross()
    {
        ShellWindow w;
        w.resize(600, 400);
        w.show();
        QCoreApplication::processEvents();
        BottomBand* band = w.bottomBand();
        QVERIFY(band);
        const QImage img = band->grab().toImage();
        QVERIFY2(!img.isNull() && img.width() > 100, "bottom band did not render");
        const QPoint o(0, 0);
        const int bw = img.width(), bh = img.height();

        int upper = -1, lower = -1;
        QVERIFY2(ruleRows(img, o, bw, bh, &upper, &lower), "no bottom rule row found");

        const int cellW = qMax(6, band->fontMetrics().horizontalAdvance(QLatin1Char('M')));
        int below = 0;
        for (int y = lower + 1; y < bh; ++y)
            for (int x = 0; x < cellW; ++x)
                if (isFrameWhite(img.pixelColor(o.x() + x, o.y() + y)))
                    ++below;
        QVERIFY2(below == 0,
                 qPrintable(QStringLiteral("left corner has %1 white px below the rule - "
                                           "a full-height bar is a cross, not a corner")
                                .arg(below)));
    }

    // The bottom-right corner is green (the join where the hbar column meets
    // the bottom rule), not white like the rest of the frame.
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
        const QPoint o(0, 0);
        const int bw = img.width(), bh = img.height();

        int upper = -1, lower = -1;
        QVERIFY2(ruleRows(img, o, bw, bh, &upper, &lower), "no bottom rule row found");

        const int cellW = qMax(6, band->fontMetrics().horizontalAdvance(QLatin1Char('M')));
        int green = 0;
        for (int y = 0; y <= upper; ++y)
            for (int x = bw - cellW; x < bw; ++x) {
                const QColor c = img.pixelColor(o.x() + x, o.y() + y);
                if (c.green() > 120 && c.red() < 100 && c.blue() < 100)
                    ++green;
            }
        QVERIFY2(green > 0, "bottom-right corner is not green");
    }
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    TstFrame tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "tst_frame.moc"
