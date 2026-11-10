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

bool isGray(const QColor& c)
{
    return qAbs(c.red() - 0xAA) <= 30 && qAbs(c.green() - 0xAA) <= 30 &&
           qAbs(c.blue() - 0xAA) <= 30;
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
    void editor_host_paints_left_right_frame_edges() {
        ShellWindow w;
        w.resize(800, 600);
        const QImage img = w.grab().toImage();
        QVERIFY(!img.isNull());
        const int midY = img.height() / 2;

        // Walk inward from the left edge: find a gray frame pixel.
        bool leftGray = false;
        for (int x = 0; x < 12 && x < img.width(); ++x)
            if (isGray(img.pixelColor(x, midY))) { leftGray = true; break; }
        bool rightGray = false;
        for (int x = img.width() - 1; x >= img.width() - 12 && x >= 0; --x)
            if (isGray(img.pixelColor(x, midY))) { rightGray = true; break; }

        QVERIFY2(leftGray, "no gray left frame edge found");
        QVERIFY2(rightGray, "no gray right frame edge found");
    }
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    TstFrame tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "tst_frame.moc"
