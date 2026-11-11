// SPDX-License-Identifier: GPL-3.0-or-later
// Task 12: Turbo Vision grid proof.
// Renders the REAL ShellWindow (via ContentBridge, mirroring the app) at two
// window sizes and asserts the specced layout + palette on a downscaled
// 90x30 anchor grid:
//   row 0      = light-gray menu bar (black text)
//   row 29     = light-gray status line
//   mid rows   = TV window-blue #1A1AAA editor field
//   palette    = yellow text present
// The window stays resizable (two sizes prove it); 90x30 is only the anchor.
#include <QtTest>
#include <QApplication>
#include <QImage>
#include <QColor>
#include "src/shell/ShellWindow.h"
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

        // Middle of the editor = TV window blue #1A1AAA.
        const QColor mid = bandColor(scaled, 15);
        QVERIFY2(qAbs(mid.red() - 0x1A) <= 26 && qAbs(mid.green() - 0x1A) <= 26 &&
                 qAbs(mid.blue() - 0xAA) <= 30,
                 qPrintable(QStringLiteral("row 15 is not TV blue: #%1%2%3")
                                .arg(mid.red(), 2, 16, QLatin1Char('0'))
                                .arg(mid.green(), 2, 16, QLatin1Char('0'))
                                .arg(mid.blue(), 2, 16, QLatin1Char('0'))));

        // Full-res palette presence.
        QVERIFY2(countColor(frame, QColor(0x1A, 0x1A, 0xAA), 6) > 5000,
                 "TV window blue #1A1AAA not dominant in frame");
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
        QVERIFY2(qAbs(mid.blue() - 0xAA) <= 30 && qAbs(mid.red() - 0x1A) <= 26,
                 qPrintable(QStringLiteral("resized row 15 not TV blue: #%1%2%3")
                                .arg(mid.red(), 2, 16, QLatin1Char('0'))
                                .arg(mid.green(), 2, 16, QLatin1Char('0'))
                                .arg(mid.blue(), 2, 16, QLatin1Char('0'))));
    }
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    TstTvGrid tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "tst_tv_grid.moc"
