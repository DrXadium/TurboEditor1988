// SPDX-License-Identifier: GPL-3.0-or-later
// Task 12: Turbo Vision title-line tab strip contract test.
// Proves: (1) the strip paints a green [■] close box and a green [↕] zoom box;
// (2) tabs render as title-line segments with the active tab highlighted;
// (3) clicking a tab emits tabActivated(index).
#include <QtTest>
#include <QImage>
#include <QPainter>
#include <QMouseEvent>
#include "src/shell/TabStrip.h"
#include "src/shell/ShellWindow.h"

namespace {
const QColor kGreen(0x00, 0xAA, 0x00);
const QColor kBlue(0x00, 0x00, 0xAA);
const QColor kBandGrey(0x55, 0x55, 0x55);   // stipple ink of the tab row
const QColor kWhite(0xFF, 0xFF, 0xFF);

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

    void paints_green_close_and_zoom_boxes() {
        TabStrip ts;
        ts.setTabs(QStringList() << "untitled" << "notes", 0);
        ts.resize(600, 26);
        const QImage img = ts.grab().toImage();
        QVERIFY(!img.isNull());
        // Both boxes are green; count green pixels (two ~18x16 boxes).
        const int green = countColor(img, kGreen, 24);
        QVERIFY2(green > 120,
                 qPrintable(QStringLiteral("green close/zoom box pixels=%1, expected >120").arg(green)));

        // Boxes are present at their queried geometry (left + right).
        QVERIFY(ts.closeBoxRect().left() < ts.zoomBoxRect().left());
        QVERIFY(ts.closeBoxRect().left() >= 0);
        QVERIFY(ts.zoomBoxRect().right() <= ts.width());
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

    // TV-revision spec 3: tab titles sit in the top border row, WHITE, the
    // active tab ("hostname" when active=1) highlighted on a gray plate with
    // white text; inactive labels are white on blue (not gray).
    void tab_titles_are_white_active_is_gray_plate() {
        TabStrip ts;
        ts.setTabs(QStringList() << "untitled" << "hostname" << "os-release", 1);
        ts.resize(640, 26);
        const QImage img = ts.grab().toImage();
        QVERIFY(!img.isNull());
        const QVector<QRect> rects = ts.tabRects();
        QCOMPARE(rects.size(), 3);

        auto countIn = [&](const QRect& r, const QColor& t, int tol) {
            int n = 0;
            for (int y = r.top(); y <= r.bottom(); ++y)
                for (int x = r.left(); x <= r.right(); ++x) {
                    if (x < 0 || y < 0 || x >= img.width() || y >= img.height())
                        continue;
                    const QColor c = img.pixelColor(x, y);
                    if (qAbs(c.red() - t.red()) <= tol && qAbs(c.green() - t.green()) <= tol &&
                        qAbs(c.blue() - t.blue()) <= tol)
                        ++n;
                }
            return n;
        };
        const QColor white(255, 255, 255);
        const QColor gray(0xAA, 0xAA, 0xAA);
        const QColor blue(0x00, 0x00, 0xAA);

        // Inactive tab "untitled" (index 0): white text, blue behind (no plate).
        const int untitledWhite = countIn(rects[0], white, 24);
        QVERIFY2(untitledWhite >= 3,
                 qPrintable(QStringLiteral("inactive tab 'untitled' has %1 white px (expected >=3)")
                                .arg(untitledWhite)));
        QVERIFY2(countIn(rects[0], gray, 16) == 0,
                 "inactive tab has a gray plate — must be white on blue");

        // Active tab "hostname" (index 1): gray plate + WHITE text on it.
        const int hostnameGray = countIn(rects[1], gray, 16);
        QVERIFY2(hostnameGray >= 20,
                 qPrintable(QStringLiteral("active tab plate not gray #AAAAAA: %1 px").arg(hostnameGray)));
        // TV-revision2 spec 2: active tab = BLACK text on the gray plate.
        const int hostnameBlack = countIn(rects[1], QColor(0, 0, 0), 24);
        QVERIFY2(hostnameBlack >= 3,
                 qPrintable(QStringLiteral("active tab text not black: %1 px").arg(hostnameBlack)));
    }

    // TV-revision spec 3: green [■] close box top-left and [↕] zoom box
    // top-right, re-pinned to #00AA00.
    void close_zoom_boxes_are_tv_green() {
        TabStrip ts;
        ts.setTabs(QStringList() << "untitled", 0);
        ts.resize(600, 26);
        const QImage img = ts.grab().toImage();
        QVERIFY(!img.isNull());
        const QColor kGreen(0x00, 0xAA, 0x00);
        auto greenIn = [&](const QRect& box) {
            int n = 0;
            for (int y = box.top(); y <= box.bottom(); ++y)
                for (int x = box.left(); x <= box.right(); ++x) {
                    if (x < 0 || y < 0 || x >= img.width() || y >= img.height())
                        continue;
                    const QColor c = img.pixelColor(x, y);
                    if (qAbs(c.red() - kGreen.red()) <= 24 &&
                        qAbs(c.green() - kGreen.green()) <= 24 &&
                        qAbs(c.blue() - kGreen.blue()) <= 24)
                        ++n;
                }
            return n;
        };
        const int closeG = greenIn(ts.closeBoxRect());
        const int zoomG = greenIn(ts.zoomBoxRect());
        QVERIFY2(closeG >= 10,
                 qPrintable(QStringLiteral("close box green #00AA00 px=%1, expected >=10").arg(closeG)));
        QVERIFY2(zoomG >= 6,
                 qPrintable(QStringLiteral("zoom box green #00AA00 px=%1, expected >=6").arg(zoomG)));
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
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    TstTabStrip tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "tst_tabstrip.moc"
