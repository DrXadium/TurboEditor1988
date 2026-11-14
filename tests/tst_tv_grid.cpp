// SPDX-License-Identifier: GPL-3.0-or-later
// Task 12: Turbo Vision grid proof.
// Renders the REAL ShellWindow (via ContentBridge, mirroring the app) at two
// window sizes and asserts the specced layout + palette on a downscaled
// 90x30 anchor grid:
//   row 0      = light-gray menu bar (black text)
//   row 29     = light-gray status line
//   mid rows   = TV window-blue #0000AA editor field
//   palette    = yellow text present
// The window stays resizable (two sizes prove it); 90x30 is only the anchor.
#include <QtTest>
#include <QApplication>
#include <QMenuBar>
#include <QImage>
#include <QColor>
#include "src/shell/ShellWindow.h"
#include "src/shell/TabStrip.h"
#include "src/shell/BottomBand.h"
#include "src/shell/FKeyStatusBar.h"
#include "src/bridge/ContentBridge.h"

namespace {
// Dominant color of the downscaled image at grid cell (row) across its width:
// returns the modal color in that band.
QColor bandColor(const QImage& scaled, int row)
{
    QMap<QRgb, int> hist;
    const int y0 = row * scaled.height() / 30;
    const int y1 = (row + 1) * scaled.height() / 30;
    for (int y = y0; y < y1; ++y)
        for (int x = 0; x < scaled.width(); ++x)
            hist[scaled.pixel(x, y)]++;
    QRgb best = 0; int bestN = -1;
    for (auto it = hist.begin(); it != hist.end(); ++it)
        if (it.value() > bestN) { bestN = it.value(); best = it.key(); }
    return QColor::fromRgb(best);
}

// Count pixels close to a target across the frame.
int countColor(const QImage& img, const QColor& t, int tol)
{
    int n = 0;
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x) {
            const QColor c = img.pixelColor(x, y);
            if (qAbs(c.red() - t.red()) <= tol &&
                qAbs(c.green() - t.green()) <= tol &&
                qAbs(c.blue() - t.blue()) <= tol)
                ++n;
        }
    return n;
}

QImage renderShell(ShellWindow& w, const QSize& size)
{
    w.resize(size);
    ContentBridge b;
    b.setShellWindow(&w);
    b.update();
    return b.frame();
}
} // namespace

class TstTvGrid : public QObject
{
    Q_OBJECT
private slots:
    // Anchor size (the 90x30 reference proportions): full palette + layout.
    void anchor_grid_layout_and_palette() {
        ShellWindow w;
        const QImage frame = renderShell(w, QSize(900, 600));
        QVERIFY2(!frame.isNull(), "frame is null");

        // Downscale to the 90x30 anchor grid.
        const QImage scaled = frame.scaled(90, 30, Qt::IgnoreAspectRatio,
                                           Qt::SmoothTransformation);
        QCOMPARE(scaled.size(), QSize(90, 30));

        // Row 0 = light-gray menu bar.
        const QColor menuBg = bandColor(scaled, 0);
        QVERIFY2(qAbs(menuBg.red() - 0xAA) <= 28 && qAbs(menuBg.green() - 0xAA) <= 28 &&
                 qAbs(menuBg.blue() - 0xAA) <= 28,
                 qPrintable(QStringLiteral("row 0 is not gray menu bar: #%1%2%3")
                                .arg(menuBg.red(), 2, 16, QLatin1Char('0'))
                                .arg(menuBg.green(), 2, 16, QLatin1Char('0'))
                                .arg(menuBg.blue(), 2, 16, QLatin1Char('0'))));

        // Row 29 = light-gray status line.
        const QColor statusBg = bandColor(scaled, 29);
        QVERIFY2(qAbs(statusBg.red() - 0xAA) <= 28 && qAbs(statusBg.green() - 0xAA) <= 28 &&
                 qAbs(statusBg.blue() - 0xAA) <= 28,
                 qPrintable(QStringLiteral("row 29 is not gray status line: #%1%2%3")
                                .arg(statusBg.red(), 2, 16, QLatin1Char('0'))
                                .arg(statusBg.green(), 2, 16, QLatin1Char('0'))
                                .arg(statusBg.blue(), 2, 16, QLatin1Char('0'))));

        // Middle of the editor = TV window blue #0000AA.
        const QColor mid = bandColor(scaled, 15);
        QVERIFY2(qAbs(mid.red() - 0x00) <= 8 && qAbs(mid.green() - 0x00) <= 8 &&
                 qAbs(mid.blue() - 0xAA) <= 30,
                 qPrintable(QStringLiteral("row 15 is not TV blue: #%1%2%3")
                                .arg(mid.red(), 2, 16, QLatin1Char('0'))
                                .arg(mid.green(), 2, 16, QLatin1Char('0'))
                                .arg(mid.blue(), 2, 16, QLatin1Char('0'))));

        // Full-res palette presence.
        QVERIFY2(countColor(frame, QColor(0x00, 0x00, 0xAA), 6) > 5000,
                 "TV window blue #0000AA not dominant in frame");
        QVERIFY2(countColor(frame, QColor(0xAA, 0xAA, 0xAA), 8) > 2000,
                 "gray bars #AAAAAA not present in frame");
        // TV-revision spec 4+7: yellow lives ONLY in the editor text area —
        // the status line and chrome carry no yellow (spec 7). Assert the
        // frame's yellow is present and confined to the central editor area.
        {
            const QPoint cOrigin = w.centralWidget()->mapTo(&w, QPoint(0, 0));
            const QRect cre(cOrigin, w.centralWidget()->size());
            int yCentral = 0, yElse = 0;
            for (int yy = 0; yy < frame.height(); ++yy)
                for (int xx = 0; xx < frame.width(); ++xx) {
                    const QColor c = frame.pixelColor(xx, yy);
                    if (qAbs(c.red() - 0xFF) <= 40 && qAbs(c.green() - 0xFF) <= 40
                        && qAbs(c.blue() - 0x55) <= 40) {
                        if (cre.contains(QPoint(xx, yy))) ++yCentral;
                        else ++yElse;
                    }
                }
            QVERIFY2(yCentral > 0,
                     "no yellow editor text #FFFF55 in the central area");
            QVERIFY2(yElse == 0,
                     qPrintable(QStringLiteral("yellow outside the editor area: %1 px")
                                    .arg(yElse)));
            QVERIFY2(yCentral > 40,
                     qPrintable(QStringLiteral("editor-area yellow too thin: %1 px")
                                    .arg(yCentral)));
        }
    }

    // Resizable: a second, different size still shows gray bars + blue field.
    void resized_still_tv_layout() {
        ShellWindow w;
        const QImage frame = renderShell(w, QSize(640, 480));
        QVERIFY(!frame.isNull());
        QCOMPARE(frame.size(), QSize(640, 480));

        const QImage scaled = frame.scaled(90, 30, Qt::IgnoreAspectRatio,
                                           Qt::SmoothTransformation);
        const QColor menuBg = bandColor(scaled, 0);
        const QColor mid = bandColor(scaled, 15);
        QVERIFY2(qAbs(menuBg.red() - 0xAA) <= 28 && qAbs(menuBg.green() - 0xAA) <= 28,
                 qPrintable(QStringLiteral("resized row 0 not gray: #%1%2%3")
                                .arg(menuBg.red(), 2, 16, QLatin1Char('0'))
                                .arg(menuBg.green(), 2, 16, QLatin1Char('0'))
                                .arg(menuBg.blue(), 2, 16, QLatin1Char('0'))));
        QVERIFY2(qAbs(mid.blue() - 0xAA) <= 30 && qAbs(mid.red() - 0x00) <= 8,
                 qPrintable(QStringLiteral("resized row 15 not TV blue: #%1%2%3")
                                .arg(mid.red(), 2, 16, QLatin1Char('0'))
                                .arg(mid.green(), 2, 16, QLatin1Char('0'))
                                .arg(mid.blue(), 2, 16, QLatin1Char('0'))));
    }

    // TV-revision2 spec 8: top-to-bottom row order is
    //   menu bar -> window top border (tabs) -> editor rows -> bottom border
    //   (position + h-scrollbar) -> status line,
    // with no black voids and nothing clipped at any edge.
    void layout_order_menu_border_editor_border_status() {
        ShellWindow w;
        const QImage frame = renderShell(w, QSize(900, 600));
        QVERIFY2(!frame.isNull(), "frame is null");
        const int W = frame.width(), H = frame.height();

        // Widget geometry gives the bands; assert the order first. Children
        // live in their parent's coordinate space (TabStrip is inside the
        // central host), so map every band into WINDOW coordinates.
        auto* menu = w.menuBar();
        TabStrip* tabs = w.tabStrip();
        BottomBand* band = w.bottomBand();
        FKeyStatusBar* status = w.fkeyStatusBar();
        QVERIFY(menu && tabs && band && status);
        const auto rectInWindow = [&](QWidget* child) {
            return QRect(child->mapTo(&w, QPoint(0, 0)), child->size());
        };
        const QRect menuR = rectInWindow(menu);
        const QRect tabsR = rectInWindow(tabs);
        const QRect bandR = rectInWindow(band);
        const QRect statusR = rectInWindow(status);
        QVERIFY2(menuR.bottom() < tabsR.top(),
                 qPrintable(QStringLiteral("menu bar (bottom=%1) not above the window "
                                           "top border (top=%2)")
                                .arg(menuR.bottom()).arg(tabsR.top())));
        QVERIFY2(tabsR.top() < bandR.top(),
                 qPrintable(QStringLiteral("top border (top=%1) not above the bottom "
                                           "border (top=%2)")
                                .arg(tabsR.top()).arg(bandR.top())));
        QVERIFY2(bandR.top() < statusR.top(),
                 qPrintable(QStringLiteral("bottom border (top=%1) not above the status "
                                           "line (top=%2)")
                                .arg(bandR.top()).arg(statusR.top())));

        const auto bandDominant = [&](int y0, int y1, QColor want, int tol,
                                      const char* what) {
            int hit = 0, total = 0;
            y0 = qMax(0, y0); y1 = qMin(H - 1, y1);
            for (int y = y0; y <= y1; ++y)
                for (int x = 0; x < W; ++x) {
                    const QColor c = frame.pixelColor(x, y);
                    ++total;
                    if (qAbs(c.red() - want.red()) <= tol
                        && qAbs(c.green() - want.green()) <= tol
                        && qAbs(c.blue() - want.blue()) <= tol)
                        ++hit;
                }
            QVERIFY2(total > 0 && hit * 2 >= total,
                     qPrintable(QStringLiteral("%1 band not dominant (#%2%3%4): %5/%6")
                                    .arg(QString::fromLatin1(what),
                                         QString::number(want.red(), 16),
                                         QString::number(want.green(), 16),
                                         QString::number(want.blue(), 16))
                                    .arg(hit).arg(total)));
        };
        // Row order, top to bottom.
        bandDominant(0, menuR.bottom(), QColor(0xAA, 0xAA, 0xAA), 28, "menu");
        // Task 19 (mock note 2): the tab row is the scrollbar track's stipple
        // design in GREY ink instead of teal, so neither ink holds a majority
        // on its own - the two together fill the band. Everything from the
        // document's border row down is still the blue field.
        {
            int hit = 0, total = 0;
            const QColor a(0x00, 0x00, 0xAA), b(0x55, 0x55, 0x55);
            for (int y = qMax(0, tabsR.top()); y <= qMin(H - 1, tabsR.bottom()); ++y)
                for (int x = 0; x < W; ++x) {
                    const QColor c = frame.pixelColor(x, y);
                    ++total;
                    const bool inA = qAbs(c.red() - a.red()) <= 8 &&
                                     qAbs(c.green() - a.green()) <= 8 &&
                                     qAbs(c.blue() - a.blue()) <= 8;
                    const bool inB = qAbs(c.red() - b.red()) <= 8 &&
                                     qAbs(c.green() - b.green()) <= 8 &&
                                     qAbs(c.blue() - b.blue()) <= 8;
                    if (inA || inB)
                        ++hit;
                }
            QVERIFY2(total > 0 && hit * 20 >= total * 17,
                     qPrintable(QStringLiteral("tab row is not the grey-on-blue dither: "
                                               "%1/%2 px in palette")
                                    .arg(hit).arg(total)));
        }
        bandDominant(tabsR.bottom() + 1, bandR.top() - 1, QColor(0x00, 0x00, 0xAA), 28,
                     "editor");
        bandDominant(bandR.top(), bandR.bottom(), QColor(0x00, 0x00, 0xAA), 28,
                     "bottom-border");
        bandDominant(statusR.top(), H - 1, QColor(0xAA, 0xAA, 0xAA), 28, "status");

        // Top and bottom borders are drawn: white glyphs present in both bands.
        const auto whiteIn = [&](int y0, int y1) {
            int n = 0;
            for (int y = qMax(0, y0); y <= qMin(H - 1, y1); ++y)
                for (int x = 0; x < W; ++x) {
                    const QColor c = frame.pixelColor(x, y);
                    if (c.red() > 235 && c.green() > 235 && c.blue() > 235)
                        ++n;
                }
            return n;
        };
        // Task 19: the top border row is the DOCUMENT's ╔ ═ ╗ row below the tab
        // row - the tab row carries no rule of its own any more.
        auto* titleRow = w.findChild<QWidget*>(QStringLiteral("editorTitleRow"));
        QVERIFY2(titleRow, "document border row not found");
        const QRect titleR = rectInWindow(titleRow);
        QVERIFY2(whiteIn(titleR.top(), titleR.bottom()) > 20,
                 "document top border row (double lines/corners) not drawn");
        QVERIFY2(whiteIn(bandR.top(), bandR.bottom()) > 20,
                 "window bottom border (double lines/position) not drawn");

        // No black voids: black may only appear as glyphs/shadow accents, not
        // as regions (spec 1 bans black inside the window except shadows).
        int black = 0;
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x) {
                const QColor c = frame.pixelColor(x, y);
                if (c.red() < 40 && c.green() < 40 && c.blue() < 40)
                    ++black;
            }
        QVERIFY2(black * 50 < W * H,
                 qPrintable(QStringLiteral("black voids inside the window: %1 px")
                                .arg(black)));

        // Nothing clipped: the frame reaches all four edges (top menu row gray,
        // bottom status row gray, and the border columns painted at x=0/x=W-1).
        const auto isGray = [](const QColor& c) {
            return qAbs(c.red() - 0xAA) <= 28 && qAbs(c.green() - 0xAA) <= 28
                && qAbs(c.blue() - 0xAA) <= 28;
        };
        QVERIFY2(isGray(frame.pixelColor(0, 1)) && isGray(frame.pixelColor(W - 1, 1)),
                 "menu row does not reach both edges");
        QVERIFY2(isGray(frame.pixelColor(0, H - 2)) && isGray(frame.pixelColor(W - 1, H - 2)),
                 "status row does not reach both edges");
    }
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    TstTvGrid tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "tst_tv_grid.moc"
