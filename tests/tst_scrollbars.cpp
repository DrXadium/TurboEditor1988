// SPDX-License-Identifier: GPL-3.0-or-later
// Task 4: Turbo Vision text-mode scrollbar contract test.
// The scrollbars must render DOS-style ASCII glyphs: arrow blocks (▲▼ / ◄►)
// at the ends, a textured track (▒), and a solid thumb (█) — not plain color
// bars. Driven by the TextScrollBar subclass (BorlandStyle installs it).
#include <QtTest>
#include <QImage>
#include <QColor>
#include <QScrollBar>
#include <QMenuBar>
#include <QMenu>
#include "src/shell/TextScrollBar.h"
#include "src/shell/ShellWindow.h"
#include "src/shell/TabStrip.h"
#include "src/shell/BottomBand.h"
#include "src/editor/EditorTabManager.h"
#include <ktexteditor/view.h>
#include <ktexteditor/document.h>
#include <QScrollBar>

namespace {
// Teal track (#00AAAA) and green handle (#00AA00) — the TV scrollbar palette.
bool isTeal(const QColor& c)
{
    return c.blue() > 140 && c.green() > 140 && c.red() < 120;
}
// Off-palette light teal (#4FCFCF) — banned by the TV-revision spec (only the
// 8 named colors are allowed: blue/gray/yellow/white/teal/green/red/black).
bool isOffPaletteTeal(const QColor& c)
{
    return qAbs(c.red() - 0x4F) <= 14 && qAbs(c.green() - 0xCF) <= 14 &&
           qAbs(c.blue() - 0xCF) <= 14;
}
// Glyph ink pixel (window-blue arrow/body over teal/green - spec 1 bans
// pure-black pixels inside the window, so ink is drawn in #0000AA).
bool isInk(const QColor& c)
{
    return c.blue() > 110 && c.red() < 90 && c.green() < 90;
}
} // namespace

class TstScrollbars : public QObject
{
    Q_OBJECT
private slots:
    // A vertical TextScrollBar renders a down-arrow glyph in its add-line
    // (bottom) button region — dark pixels forming the ▼ arrow on the teal bg.
    void vertical_scrollbar_draws_arrow_glyph() {
        TextScrollBar v(Qt::Vertical);
        v.setRange(0, 100);
        v.setValue(0);
        v.resize(14, 200);
        v.show();
        QCoreApplication::processEvents();
        const QImage img = v.grab().toImage();
        QVERIFY(!img.isNull());

        // add-line (bottom button) region: bottom ~14px.
        const int btnH = 14;
        int darkInBottom = 0;
        for (int y = v.height() - btnH; y < v.height(); ++y)
            for (int x = 2; x < v.width() - 2; ++x) {
                if (x < 0 || y < 0 || x >= img.width() || y >= img.height())
                    continue;
                if (isInk(img.pixelColor(x, y)))
                    ++darkInBottom;
            }
        QVERIFY2(darkInBottom >= 4,
                 qPrintable(QStringLiteral(
                     "vertical scrollbar add-line has no arrow glyph (%1 dark px)")
                                .arg(darkInBottom)));
    }

    // A horizontal TextScrollBar renders a right-arrow glyph in its add-line.
    void horizontal_scrollbar_draws_arrow_glyph() {
        TextScrollBar h(Qt::Horizontal);
        h.setRange(0, 100);
        h.setValue(0);
        h.resize(300, 14);
        h.show();
        QCoreApplication::processEvents();
        const QImage img = h.grab().toImage();
        QVERIFY(!img.isNull());

        const int btnW = 14;
        int darkInRight = 0;
        for (int y = 2; y < h.height() - 2; ++y)
            for (int x = h.width() - btnW; x < h.width(); ++x) {
                if (x < 0 || y < 0 || x >= img.width() || y >= img.height())
                    continue;
                if (isInk(img.pixelColor(x, y)))
                    ++darkInRight;
            }
        QVERIFY2(darkInRight >= 4,
                 qPrintable(QStringLiteral(
                     "horizontal scrollbar add-line has no arrow glyph (%1 dark px)")
                                .arg(darkInRight)));
    }

    // The scrollbar track is the TV teal (#00AAAA), not the default gray.
    void scrollbar_track_is_tv_teal() {
        TextScrollBar v(Qt::Vertical);
        v.setRange(0, 100);
        v.setValue(50);
        v.resize(14, 200);
        v.show();
        QCoreApplication::processEvents();
        const QImage img = v.grab().toImage();
        QVERIFY(!img.isNull());
        // Sample the track just above the thumb (mid-upper region).
        int teal = 0;
        for (int y = 40; y < 60; ++y)
            for (int x = 1; x < v.width() - 1; ++x)
                if (isTeal(img.pixelColor(x, y)))
                    ++teal;
        QVERIFY2(teal > 20,
                 qPrintable(QStringLiteral(
                     "scrollbar track is not TV teal (#00AAAA): %1 teal px").arg(teal)));
    }
    // TV-revision spec 5: the scrollbar palette is TV-PURE — no off-palette
    // #4FCFCF anywhere; track dithers teal over blue; thumb green; arrow-block
    // bases teal with a black glyph.
    void scrollbar_palette_is_tv_pure() {
        TextScrollBar v(Qt::Vertical);
        v.setRange(0, 100);
        v.setValue(50);
        v.resize(14, 200);
        v.show();
        QCoreApplication::processEvents();
        const QImage img = v.grab().toImage();
        QVERIFY(!img.isNull());

        const QColor kTeal(0x00, 0xAA, 0xAA);
        const QColor kGreen(0x00, 0xAA, 0x00);
        const QColor kBlue(0x00, 0x00, 0xAA);
        int banned = 0, teal = 0, green = 0, blue = 0;
        auto near = [](const QColor& c, const QColor& t, int tol) {
            return qAbs(c.red() - t.red()) <= tol && qAbs(c.green() - t.green()) <= tol &&
                   qAbs(c.blue() - t.blue()) <= tol;
        };
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x) {
                const QColor c = img.pixelColor(x, y);
                if (isOffPaletteTeal(c))
                    ++banned;
                if (near(c, kTeal, 24))
                    ++teal;
                if (near(c, kGreen, 24))
                    ++green;
                if (near(c, kBlue, 24))
                    ++blue;
            }
        QVERIFY2(banned == 0,
                 qPrintable(QStringLiteral("off-palette #4FCFCF present: %1 px (spec forbids it)")
                                .arg(banned)));
        // Track dithers teal over blue: both present.
        QVERIFY2(teal > 50, qPrintable(QStringLiteral("track lacks teal dither: %1 teal px").arg(teal)));
        QVERIFY2(blue > 20,
                 qPrintable(QStringLiteral("track lacks blue dither base: %1 blue px").arg(blue)));
        // Thumb is green.
        QVERIFY2(green > 50, qPrintable(QStringLiteral("thumb not green: %1 green px").arg(green)));
    }

    // TV-revision spec 5: the right border always carries the VERTICAL
    // scrollbar — with a short document (no overflow, no horizontal need) the
    // vertical bar is present and visible. Probe-verified: the doc bar is the
    // one under KTextEditor::ViewPrivate with a real range; Kate exposes no
    // scrollbar-policy API on the public View (see .relay/vbar2.cpp).
    void kate_doc_vertical_hidden_by_window_bar() {
        ShellWindow shell;   // the bar's isVisible() depends on the real shell window
        shell.show();
        EditorTabManager m;
        m.newDocument();
        auto* v = m.activeView();
        QVERIFY(v);
        v->document()->setText(QStringLiteral("reibot\n"));
        v->resize(400, 200);
        v->show();
        // The hide is deliberately deferred one event-loop turn (mutating a
        // widget mid-Show is the ScrollBarInstaller segfault lesson) and Kate
        // may re-show the bar once more in between — let the
        // show/hide/reshow chain settle (bounded).
        const auto docVertShownCount = [&]() {
            int shown = 0;
            for (QScrollBar* b : v->findChildren<QScrollBar*>())
                if (b->orientation() == Qt::Vertical && b->width() >= 40
                    && b->height() >= 100 && b->isVisibleTo(v) && !b->isHidden())
                    ++shown;
            return shown;
        };
        for (int turn = 0; turn < 8 && docVertShownCount() > 0; ++turn)
            QCoreApplication::processEvents();
        QCOMPARE(v->document()->lines(), 2);

        int docVert = 0, docVertShown = 0;
        for (QScrollBar* b : v->findChildren<QScrollBar*>()) {
            if (b->orientation() != Qt::Vertical)
                continue;
            if (b->width() >= 40 && b->height() >= 100) {   // the document bar
                ++docVert;
                // isVisibleTo(view): shown AND not hidden, independent of whether
                // an ancestor top-level window is currently on-screen.
                if (b->isVisibleTo(v) && !b->isHidden())
                    ++docVertShown;
            }
        }
        QVERIFY2(docVert >= 1, "no document vertical scrollbar found in the view");
        // TV-revision2 spec 1 hijacks the border: Kate's own document
        // vertical is HIDDEN — the 1-cell window-border bar replaces it.
        QVERIFY2(docVertShown == 0,
                 qPrintable(QStringLiteral("Kate doc vertical still visible: %1/%2 shown")
                                .arg(docVertShown).arg(docVert)));
    }

    // TV-revision spec (minimap off by default): the Show Minimap action starts
    // unchecked and the toggle roundtrip still works.
    void minimap_hidden_by_default() {
        ShellWindow w;
        QMenuBar* mb = w.menuBar();
        QAction* mm = nullptr;
        for (QAction* a : mb->actions())
            if (a->menu())
                for (QAction* sub : a->menu()->findChildren<QAction*>())
                    if (sub->objectName() == QLatin1String("actionViewMinimap"))
                        mm = sub;
        QVERIFY2(mm, "actionViewMinimap not found");
        QVERIFY2(!mm->isChecked(), "Show Minimap starts checked — spec wants it off by default");
    }

    // App-wide wiring: a plain QScrollBar (as KTextEditor/QScrollArea create)
    // is converted to a TextScrollBar so the DOS glyphs appear everywhere.
    void plain_qscrollbar_is_converted_to_text_scrollbar() {
        ShellWindow w;   // installing BorlandStyle installs the converter
        w.show();
        QCoreApplication::processEvents();
        auto* sb = new QScrollBar(Qt::Vertical, &w);
        sb->resize(14, 60);
        sb->show();
        QCoreApplication::processEvents();
        // The installer (event filter) should have swapped it for a
        // TextScrollBar, or reparented one in its place.
        bool found = false;
        const auto texts = w.findChildren<TextScrollBar*>();
        for (TextScrollBar* t : texts)
            if (t != sb)
                found = true;
        QVERIFY2(found || qobject_cast<TextScrollBar*>(sb),
                 "plain QScrollBar was not converted to TextScrollBar");
        delete sb;
    }

    // TV-revision2 spec 3: the bottom band's horizontal scrollbar is ONE
    // CELL tall, teal ◄ / ► end cells, a single-cell teal ■ thumb near the
    // left, a fine teal-on-blue ▒ texture (NOT the coarse 4-px checker), and
    // contains ZERO green pixels.
    void horizontal_scrollbar_is_one_cell_tall() {
        BottomBand band;
        band.resize(640, 26);
        band.show();
        QCoreApplication::processEvents();
        QScrollBar* hb = band.horizontalBar();
        QVERIFY2(hb, "bottom band horizontal bar missing");

        // One cell tall, bottom-aligned onto the border rows.
        const int cell = band.fontMetrics().height();
        QVERIFY2(qAbs(hb->height() - cell) <= 2,
                 qPrintable(QStringLiteral("hbar height=%1, one cell=%2")
                                .arg(hb->height()).arg(cell)));
        QVERIFY2(hb->mapTo(&band, QPoint(0, hb->height() - 1)).y() == band.height() - 1,
                 qPrintable(QStringLiteral("hbar bottom=%1, band bottom rows expected")
                                .arg(hb->mapTo(&band, QPoint(0, hb->height() - 1)).y())));

        const QImage img = hb->grab().toImage();
        QVERIFY(!img.isNull());
        const int w = img.width(), h = img.height();
        const auto isBlue = [](const QColor& c) {
            return c.blue() > 150 && c.red() < 90 && c.green() < 90;
        };
        const auto isGreen = [](const QColor& c) {
            return c.green() > 140 && c.red() < 90 && c.blue() < 90;
        };
        // Ends: teal cell + blue-ink ◄ / ►.
        const int thick = h;   // square end cells at this height
        int lTeal = 0, lInk = 0, rTeal = 0, rInk = 0, green = 0;
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                const QColor c = img.pixelColor(x, y);
                if (isGreen(c))
                    ++green;
                if (x < thick) {
                    if (isTeal(c)) ++lTeal;
                    if (isBlue(c)) ++lInk;
                } else if (x >= w - thick) {
                    if (isTeal(c)) ++rTeal;
                    if (isBlue(c)) ++rInk;
                }
            }
        QVERIFY2(lTeal > (thick * h) / 4,
                 qPrintable(QStringLiteral("left ◄ cell lacks teal (%1)").arg(lTeal)));
        QVERIFY2(lInk >= 3,
                 qPrintable(QStringLiteral("left ◄ lacks blue ink (%1)").arg(lInk)));
        QVERIFY2(rTeal > (thick * h) / 4,
                 qPrintable(QStringLiteral("right ► cell lacks teal (%1)").arg(rTeal)));
        QVERIFY2(rInk >= 3,
                 qPrintable(QStringLiteral("right ► lacks blue ink (%1)").arg(rInk)));
        QVERIFY2(green == 0,
                 qPrintable(QStringLiteral("green pixels in h-scrollbar: %1 (spec bans it)")
                                .arg(green)));

        // Thumb: contiguous run of FULLY teal columns inside the track,
        // single-cell long, anchored at the left of the track.
        int runStart = -1, runLen = 0, bestStart = -1, bestLen = 0;
        for (int x = thick; x < w - thick; ++x) {
            bool full = true;
            for (int y = 0; y < h && full; ++y)
                if (!isTeal(img.pixelColor(x, y)))
                    full = false;
            if (full) {
                if (runStart < 0)
                    runStart = x;
                ++runLen;
                if (runLen > bestLen) {
                    bestLen = runLen;
                    bestStart = runStart;
                }
            } else {
                runStart = -1;
                runLen = 0;
            }
        }
        QVERIFY2(bestStart >= 0, "no solid teal thumb column in the track");
        QVERIFY2(bestStart < w / 4,
                 qPrintable(QStringLiteral("thumb not near the left (x=%1)").arg(bestStart)));
        QVERIFY2(bestLen >= 6 && bestLen <= 24,
                 qPrintable(QStringLiteral("thumb is %1 px long - expected a single cell")
                                .arg(bestLen)));

        // Track texture: ▒ = 1-px teal/blue stipple (transitions present in
        // every 5 consecutive px), NOT the coarse 4-px checker.
        const int midY = h / 2;
        int midTeal = 0, midBlue = 0;
        for (int x = thick; x < w - thick; ++x) {
            const QColor c = img.pixelColor(x, midY);
            if (isTeal(c)) ++midTeal;
            if (isBlue(c)) ++midBlue;
        }
        QVERIFY2(midTeal > 0 && midBlue > 0,
                 qPrintable(QStringLiteral("track not teal-on-blue: teal=%1 blue=%2")
                                .arg(midTeal).arg(midBlue)));
        const auto texAt = [&](int x0) {
            int trans = 0;
            for (int i = 1; i < 5; ++i) {
                const QColor a = img.pixelColor(x0 + i - 1, midY);
                const QColor b = img.pixelColor(x0 + i, midY);
                if ((isTeal(a) && isBlue(b)) || (isBlue(a) && isTeal(b)))
                    ++trans;
            }
            return trans;
        };
        bool fineStipple = false;
        for (int x = thick + 1; x < w - thick - 5 && !fineStipple; ++x)
            if (texAt(x) >= 3)
                fineStipple = true;
        QVERIFY2(fineStipple,
                 "track is a coarse checker - spec 3 wants an even 1-px ▒ stipple");
    }

    // TV-revision2 spec 1: the window's RIGHT BORDER COLUMN is a 1-cell
    // vertical scrollbar flush to the right edge, spanning the frame
    // interior (row under the top-right corner .. row above the bottom band).
    void vertical_scrollbar_is_one_cell_at_window_right_edge() {
        ShellWindow shell;
        shell.resize(900, 600);
        shell.show();
        QCoreApplication::processEvents();

        auto* bar = shell.findChild<QScrollBar*>(QStringLiteral("rightScrollBar"));
        QVERIFY2(bar, "window right-border scrollbar (objectName rightScrollBar) not found");
        QCOMPARE(QString::fromLatin1(bar->metaObject()->className()),
                 QStringLiteral("RightScrollBar"));
        // One cell wide: the mono grid cell is ~12 px at the app's 16 px font.
        QVERIFY2(bar->width() <= 12,
                 qPrintable(QStringLiteral("right border bar is %1 px wide (> 1 cell)")
                                .arg(bar->width())));
        QVERIFY(bar->width() > 0);

        const QPoint tl = bar->mapTo(&shell, QPoint(0, 0));
        const int rightEdge = tl.x() + bar->width() - 1;
        QVERIFY2(rightEdge == shell.width() - 1,
                 qPrintable(QStringLiteral("bar right edge %1 != window width %2")
                                .arg(rightEdge).arg(shell.width() - 1)));

        // Under the title row (top-right corner) ... above the BottomBand.
        TabStrip* strip = shell.tabStrip();
        QVERIFY(strip);
        const QPoint stl = strip->mapTo(&shell, QPoint(0, 0));
        QVERIFY2(tl.y() == stl.y() + strip->height(),
                 qPrintable(QStringLiteral("bar top %1 not directly below title row bottom %2")
                                .arg(tl.y()).arg(stl.y() + strip->height())));
        const QPoint btl = shell.bottomBand()->mapTo(&shell, QPoint(0, 0));
        const int barBottom = bar->mapTo(&shell, QPoint(0, bar->height() - 1)).y();
        QVERIFY2(barBottom == btl.y() - 1,
                 qPrintable(QStringLiteral("bar bottom %1 does not end just above the band (%2)")
                                .arg(barBottom).arg(btl.y() - 1)));
    }

    // TV-revision2 spec 1: the border bar draws teal ▲/▼ end cells, a
    // teal-on-blue ▒ track, and a SINGLE-CELL solid teal ■ thumb near the top.
    void vertical_scrollbar_arrow_blocks_and_thumb() {
        ShellWindow shell;
        shell.resize(900, 600);
        shell.show();
        QCoreApplication::processEvents();
        auto* bar = shell.findChild<QScrollBar*>(QStringLiteral("rightScrollBar"));
        QVERIFY2(bar, "right-border scrollbar not found");

        const QImage img = bar->grab().toImage();
        QVERIFY(!img.isNull());
        const int w = img.width(), h = img.height();
        QVERIFY(w > 0 && h >= 6 * w);   // room for arrow cell + thumb + track
        const auto isBlue = [](const QColor& c) {
            return c.blue() > 150 && c.red() < 90 && c.green() < 90;
        };
        auto count = [&](int y0, int y1, bool (*pred)(const QColor&)) {
            int n = 0;
            for (int y = y0; y < y1; ++y)
                for (int x = 0; x < w; ++x)
                    if (pred(img.pixelColor(x, y)))
                        ++n;
            return n;
        };
        const int cellH = qMin(w + 6, h / 6);   // ~1 char tall end cells

        // Top cell: teal ▲ — teal background, arrow ink drawn in WINDOW BLUE
        // (spec 1: zero pure-black pixels anywhere inside the window, so the
        // glyphs cannot use black ink).
        const int topTeal = count(0, cellH, isTeal);
        const int topInk = count(0, cellH, isBlue);
        QVERIFY2(topTeal > (cellH * w) / 4,
                 qPrintable(QStringLiteral("top cell lacks teal ▲ bg (%1 teal px)").arg(topTeal)));
        QVERIFY2(topInk >= 3,
                 qPrintable(QStringLiteral("top cell lacks ▲ ink (%1 blue px)").arg(topInk)));

        // Bottom cell: teal ▼ same recipe.
        const int botTeal = count(h - cellH, h, isTeal);
        const int botInk = count(h - cellH, h, isBlue);
        QVERIFY2(botTeal > (cellH * w) / 4,
                 qPrintable(QStringLiteral("bottom cell lacks teal ▼ bg (%1 teal px)").arg(botTeal)));
        QVERIFY2(botInk >= 3,
                 qPrintable(QStringLiteral("bottom cell lacks ▼ ink (%1 blue px)").arg(botInk)));

        // Track middle: ▒ = teal dithered over blue (both present).
        const int midTeal = count(h / 2 - 1, h / 2 + 1, isTeal);
        const int midBlue = count(h / 2 - 1, h / 2 + 1, isBlue);
        QVERIFY2(midTeal > 0 && midBlue > 0,
                 qPrintable(QStringLiteral("track is not teal-on-blue dither: teal=%1 blue=%2")
                                .arg(midTeal).arg(midBlue)));

        // Thumb: contiguous run of FULLY teal rows (single ■ cell) near the top.
        int runStart = -1, runLen = 0, bestStart = -1, bestLen = 0;
        for (int y = cellH; y < h - cellH; ++y) {
            bool full = true;
            for (int x = 0; x < w && full; ++x)
                if (!isTeal(img.pixelColor(x, y)))
                    full = false;
            if (full) {
                if (runStart < 0)
                    runStart = y;
                ++runLen;
                if (runLen > bestLen) {
                    bestLen = runLen;
                    bestStart = runStart;
                }
            } else {
                runStart = -1;
                runLen = 0;
            }
        }
        QVERIFY2(bestStart >= 0, "no solid teal thumb row in the track");
        QVERIFY2(bestStart < h / 4,
                 qPrintable(QStringLiteral("thumb not near the top (y=%1)").arg(bestStart)));
        QVERIFY2(bestLen >= 8 && bestLen <= 30,
                 qPrintable(QStringLiteral("thumb is %1 px tall — expected a single ~1-cell block")
                                .arg(bestLen)));
    }
};

QTEST_MAIN(TstScrollbars)
#include "tst_scrollbars.moc"
