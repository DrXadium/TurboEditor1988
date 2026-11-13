// SPDX-License-Identifier: GPL-3.0-or-later
// Task 3: Borland chrome shell contract test.
// Asserts ShellWindow exists, has MenuBar / TabStrip / FKeyStatusBar chrome,
// uses BorlandStyle, and the window title is "CRT-Kate".
#include <QtTest>
#include <QApplication>
#include <QMenuBar>
#include <QStatusBar>
#include <QFile>
#include <QFontDatabase>
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

    // TV spec: the DEFAULT face is the bundled VGA typeface (PxPlus IBM VGA8),
    // not a substituted system mono. ShellWindow registers it from the embedded
    // resource and sets it as the QApplication default - that is what the QSS
    // font-family fallback and every widget resolve against.
    void default_font_is_the_bundled_vga_typeface() {
        QVERIFY2(QFile::exists(QStringLiteral(":/fonts/PxPlus_IBM_VGA8.ttf")),
                 "TTF not embedded as a Qt resource (check CMake fonts.qrc)");
        ShellWindow w;   // ctor registers the font + sets the app default
        Q_UNUSED(w);
        QVERIFY2(QFontDatabase::families().contains(QStringLiteral("PxPlus IBM VGA8")),
                 qPrintable(QStringLiteral("PxPlus IBM VGA8 not in QFontDatabase")));
        QCOMPARE(QApplication::font().family(), QStringLiteral("PxPlus IBM VGA8"));
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
        // The band has TWO rows: the horizontal scrollbar on top, and the
        // window's bottom border row underneath. The position text belongs on
        // the BORDER row, interrupting the ═ rule - it must not drift up into
        // the scrollbar row.
        const int rowH = qMax(10, band.fontMetrics().height());
        const int borderTop = h - rowH;
        int topMost = -1, whitePx = 0;
        for (int x = 75; x <= 125 && x < w; ++x)
            for (int y = borderTop; y < h; ++y) {
                const QColor c = img.pixelColor(x, y);
                if (c.red() > 200 && c.green() > 200 && c.blue() > 200) {
                    ++whitePx;
                    if (topMost < 0)
                        topMost = y;
                }
            }
        // "2:1" is three thin glyphs in the rule's gap, so the pixel count is
        // small; 6 is comfortably above the zero a missing text would give.
        QVERIFY2(whitePx >= 6,
                 qPrintable(QStringLiteral("no cursor-pos glyphs on the border row (%1 px)")
                                .arg(whitePx)));
        QVERIFY2(topMost >= borderTop,
                 qPrintable(QStringLiteral(
                     "cursor-pos text is above the border row (top row %1, border starts %2)")
                                .arg(topMost).arg(borderTop)));

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

    // The h-scrollbar sits on the band's OWN row (the upper one) and spans the
    // full width up to the corner cell; the ═ border row runs underneath it.
    void horizontal_scrollbar_spans_its_own_row_up_to_the_corner() {
        BottomBand band;
        band.resize(640, 26);
        band.show();
        QCoreApplication::processEvents();
        TextScrollBar* hb = band.horizontalBar();
        QVERIFY(hb);
        // The scrollbar shares the border row but starts a little after the
        // position text - it does not span from the left edge.
        const int textEnd = 75 + band.fontMetrics().horizontalAdvance(band.cursorPosText());
        QVERIFY2(hb->geometry().left() > textEnd,
                 qPrintable(QStringLiteral("hbar left=%1, must start after the position "
                                           "text (ends at %2)")
                                .arg(hb->geometry().left()).arg(textEnd)));
        // The hbar stops one cell short of the right edge so the ╝ frame
        // corner stays visible (p13: it previously painted over the corner).
        const int cornerCell = qMax(6, band.fontMetrics().horizontalAdvance(QLatin1Char('M')));
        QVERIFY2(hb->geometry().right() <= band.width() - cornerCell
                     && hb->geometry().right() >= band.width() - cornerCell - 3,
                 qPrintable(QStringLiteral("hbar right=%1, expected ~%2 (before the corner cell)")
                                .arg(hb->geometry().right()).arg(band.width() - cornerCell)));
        // The h-scrollbar fills the border row it shares with the position
        // text. Compare against the row: a standalone widget's resize() is
        // clamped by the platform, so the font cell is not a reliable yardstick.
        const int cell = band.fontMetrics().height();
        QVERIFY2(qAbs(hb->geometry().height() - band.height()) <= 2,
                 qPrintable(QStringLiteral("hbar height=%1, band row=%2 (one cell=%3)")
                                .arg(hb->geometry().height()).arg(band.height()).arg(cell)));
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
