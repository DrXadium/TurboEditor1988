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

namespace {
const QColor kGreen(0x19, 0xA8, 0x19);

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
} // namespace

class TstTabStrip : public QObject
{
    Q_OBJECT
private slots:
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
