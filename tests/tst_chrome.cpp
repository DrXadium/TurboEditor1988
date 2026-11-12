// SPDX-License-Identifier: GPL-3.0-or-later
// Task 3: Borland chrome shell contract test.
// Asserts ShellWindow exists, has MenuBar / TabStrip / FKeyStatusBar chrome,
// uses BorlandStyle, and the window title is "CRT-Kate".
#include <QtTest>
#include <QApplication>
#include <QMenuBar>
#include <QStatusBar>
#include "src/shell/ShellWindow.h"
#include "src/shell/BorlandStyle.h"
#include "src/shell/TabStrip.h"
#include "src/shell/FKeyStatusBar.h"
#include "src/shell/BottomBand.h"
#include "src/shell/TextScrollBar.h"

class TstChrome : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase() {
        if (!QApplication::instance())
            qWarning("expected QApplication (main provides it)");
    }

    void windowTitle_is_crt_kate() {
        ShellWindow w;
        QCOMPARE(w.windowTitle(), QStringLiteral("CRT-Kate"));
    }

    void has_chrome_children() {
        ShellWindow w;
        // MenuBar (top), TabStrip, FKeyStatusBar (bottom) present + central placeholder.
        QVERIFY2(w.menuBar(), "no menuBar");
        QVERIFY2(w.findChild<TabStrip*>(), "no TabStrip");
        QVERIFY2(w.findChild<FKeyStatusBar*>(), "no FKeyStatusBar");
        QVERIFY2(w.centralWidget(), "no central editor placeholder");
    }

    void uses_borland_style() {
        ShellWindow w;
        // QSS wraps the installed style in QStyleSheetStyle (which hides the
        // base), so assert on the BorlandStyle ShellWindow installed.
        QVERIFY2(w.borlandStyle(), "ShellWindow did not install a BorlandStyle");
        QCOMPARE(QString(w.borlandStyle()->metaObject()->className()),
                 QStringLiteral("BorlandStyle"));
    }

    // TV-revision spec 6: cursor position "2:1" rendered WHITE at cols 8-12
    // (x in [75,125]) inside the bottom band.
    void cursor_position_rendered_white_at_cols_8_12() {
        BottomBand band;
        band.setCursorPos(2, 1);
        band.resize(640, 26);
        QCOMPARE(band.cursorPosText(), QStringLiteral("2:1"));
        const QImage img = band.grab().toImage();
        QVERIFY(!img.isNull());
        int white = 0;
        for (int y = 0; y < img.height(); ++y)
            for (int x = 75; x <= 125 && x < img.width(); ++x) {
                const QColor c = img.pixelColor(x, y);
                if (c.red() > 235 && c.green() > 235 && c.blue() > 235)
                    ++white;
            }
        QVERIFY2(white >= 3,
                 qPrintable(QStringLiteral("no white cursor-pos text in x[75,125]: %1 px").arg(white)));
    }

    // TV-revision2 spec 3: the cursor position "L:C" is plain white text
    // SITTING ON the double-line border rows (glyph tops drop into the lower
    // half of the band), with no blue/gray backdrop box behind it (the band
    // stays flat window blue under the text).
    void cursor_position_drawn_into_double_line_border() {
        BottomBand band;
        band.setCursorPos(2, 1);
        band.resize(640, 26);
        const QImage img = band.grab().toImage();
        QVERIFY(!img.isNull());
        const int w = img.width(), h = img.height();
        const int bot = h - 1;

        // Rev2 spec 3: the pos text sits IN the double-line border (no separate
        // blue box behind it). The band is now exactly ONE text row tall, so the
        // text fills most of it: assert the glyphs are present and vertically
        // centered in the band rather than pinned to the top edge.
        int topMost = -1, whitePx = 0;
        for (int x = 75; x <= 125 && x < w; ++x)
            for (int y = 0; y < h - 2; ++y) {   // above the border lines
                const QColor c = img.pixelColor(x, y);
                if (c.red() > 200 && c.green() > 200 && c.blue() > 200) {
                    ++whitePx;
                    if (topMost < 0)
                        topMost = y;
                }
            }
        QVERIFY2(whitePx >= 12,
                 qPrintable(QStringLiteral("no cursor-pos glyphs above the border (%1 px)")
                                .arg(whitePx)));
        // p13: single-row band -> the text starts near the top but its glyph
        // block must run through the middle, not hug row 0 of a tall band.
        QVERIFY2(topMost >= 0 && topMost <= h / 2 + 4,
                 qPrintable(QStringLiteral(
                     "cursor-pos text does not sit mid-band (top row %1, band %2)")
                                .arg(topMost).arg(h)));

        // No backdrop box: the rows ABOVE the border lines in the pos area
        // stay dominated by flat window blue (a box would replace it).
        int blue = 0, total = 0;
        for (int y = 0; y < h - 2; ++y)
            for (int x = 70; x <= 134 && x < w; ++x) {
                const QColor c = img.pixelColor(x, y);
                ++total;
                if (qAbs(c.red() - 0x00) <= 40 && qAbs(c.green() - 0x00) <= 40
                    && qAbs(c.blue() - 0xAA) <= 40)
                    ++blue;
            }
        QVERIFY2(total > 0 && blue * 2 > total,
                 qPrintable(QStringLiteral("cursor-pos has a backdrop box: blue %1/%2")
                                .arg(blue).arg(total)));
    }

    // TV-revision spec 5h: the horizontal scrollbar spans from ~col 18 (x≥180)
    // to the right edge, full band height.
    void horizontal_scrollbar_spans_from_col_18_to_right() {
        BottomBand band;
        band.resize(640, 26);
        band.show();
        QCoreApplication::processEvents();
        TextScrollBar* hb = band.horizontalBar();
        QVERIFY(hb);
        QVERIFY2(hb->geometry().left() >= 180,
                 qPrintable(QStringLiteral("hbar left=%1, expected >=180 (col 18)")
                                .arg(hb->geometry().left())));
        // The hbar stops one cell short of the right edge so the ╝ frame
        // corner stays visible (p13: it previously painted over the corner).
        const int cornerCell = qMax(6, band.fontMetrics().horizontalAdvance(QLatin1Char('M')));
        QVERIFY2(hb->geometry().right() <= band.width() - cornerCell
                     && hb->geometry().right() >= band.width() - cornerCell - 3,
                 qPrintable(QStringLiteral("hbar right=%1, expected ~%2 (before the corner cell)")
                                .arg(hb->geometry().right()).arg(band.width() - cornerCell)));
        // TV-revision2 spec 3: the h-scrollbar is ONE CELL tall (the border
        // row), not the full band height.
        const int cell = band.fontMetrics().height();
        QVERIFY2(qAbs(hb->geometry().height() - cell) <= 2,
                 qPrintable(QStringLiteral("hbar height=%1, one cell=%2")
                                .arg(hb->geometry().height()).arg(cell)));
    }

    // TV-revision spec 9: row order — menu bar, window top border (tabs),
    // editor text rows, bottom border (scrollbar + info), status line. The
    // BottomBand must sit between the editor and the status line.
    void bottom_band_sits_between_editor_and_status_line() {
        ShellWindow w;
        w.resize(800, 600);
        w.show();
        QCoreApplication::processEvents();
        QMenuBar* mb = w.menuBar();
        BottomBand* band = w.bottomBand();
        FKeyStatusBar* st = w.fkeyStatusBar();
        QWidget* central = w.centralWidget();
        QVERIFY(mb && band && st && central);
        const int yMenu = mb->geometry().bottom();
        const int yCentral = central->geometry().bottom();
        const int yBand = band->geometry().top();
        const int yStatus = st->geometry().top();
        QVERIFY2(yMenu < yBand, "menu bar not above the bottom band");
        QVERIFY2(yBand < yStatus,
                 qPrintable(QStringLiteral("bottom band (top=%1) not above status line (top=%2)")
                                .arg(yBand).arg(yStatus)));
        // The band is the last row of the central area (below the editor).
        QVERIFY2(qAbs(yCentral - band->geometry().bottom()) <= 1 ||
                 band->geometry().bottom() <= yCentral + 1,
                 "band is not the bottom row of the central area");
        // Status line is the very last row (at the window's bottom edge).
        QVERIFY2(yStatus >= w.height() - 40,
                 qPrintable(QStringLiteral("status line top=%1 not near window bottom %2")
                                .arg(yStatus).arg(w.height())));
    }
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    TstChrome tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "tst_chrome.moc"
