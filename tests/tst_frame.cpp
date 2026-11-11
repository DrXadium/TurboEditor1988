// SPDX-License-Identifier: GPL-3.0-or-later
// Task 12: Turbo Vision window-frame contract test.
// Proves the editor host paints a double-line frame: left + right vertical
// edges, bottom edge (the TabStrip supplies the top edge/title line).
#include <QtTest>
#include <QImage>
#include <QColor>
#include "src/shell/ShellWindow.h"
#include "src/shell/TabStrip.h"

namespace {
const QColor kFrameGray(0xAA, 0xAA, 0xAA);
const QColor kBgBlue(0x1A, 0x1A, 0xAA);
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
        auto sampleGreenLocal = [&](const QRect& box) {
            const QPoint pts[] = { QPoint(2, 2), QPoint(box.width() - 4, 2),
                                   QPoint(2, box.height() - 4) };
            for (const QPoint& local : pts) {
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
        const bool closeGreen = sampleGreenLocal(ts->closeBoxRect());
        const bool zoomGreen = sampleGreenLocal(ts->zoomBoxRect());
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
        QWidget* central = w.centralWidget();
        QVERIFY(ts && central);
        const QImage img = w.grab().toImage();
        QVERIFY(!img.isNull());

        const QPoint tsOrigin = ts->mapTo(&w, QPoint(0, 0));
        const QPoint cOrigin = central->mapTo(&w, QPoint(0, 0));

        // Frame-box corners in shell coordinates:
        //   top-left/top-right: the title line's left/right ends.
        //   bottom-left/bottom-right: the editor host's bottom corners.
        const QPoint corners[4] = {
            tsOrigin + QPoint(3, ts->height() - 4),                 // top-left
            tsOrigin + QPoint(ts->width() - 4, ts->height() - 4),   // top-right
            cOrigin + QPoint(3, central->height() - 4),             // bottom-left
            cOrigin + QPoint(central->width() - 4, central->height() - 4)  // bottom-right
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
                    if (isFrameWhite(img.pixelColor(x, y)))
                        found = true;
                }
            QVERIFY2(found, qPrintable(QStringLiteral("frame %1 corner missing WHITE box at (%2,%3)")
                                           .arg(names[i]).arg(c.x()).arg(c.y())));
        }
    }

    void editor_host_paints_left_right_frame_edges() {
        ShellWindow w;
        w.resize(800, 600);
        const QImage img = w.grab().toImage();
        QVERIFY(!img.isNull());
        const int midY = img.height() / 2;

        // Walk inward from the left edge: find a WHITE frame pixel (spec 3).
        bool leftWhite = false;
        for (int x = 0; x < 12 && x < img.width(); ++x)
            if (isFrameWhite(img.pixelColor(x, midY))) { leftWhite = true; break; }
        bool rightWhite = false;
        for (int x = img.width() - 1; x >= img.width() - 12 && x >= 0; --x)
            if (isFrameWhite(img.pixelColor(x, midY))) { rightWhite = true; break; }

        QVERIFY2(leftWhite, "no WHITE left frame edge found");
        QVERIFY2(rightWhite, "no WHITE right frame edge found");
    }
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    TstFrame tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "tst_frame.moc"
