// SPDX-License-Identifier: GPL-3.0-or-later
// Task 4: ContentBridge contract test.
// After update(), the bridged frame must contain ShellWindow's navy desktop
// (the bridge proves the widget tree renders into the texture the QML CRT
// scene will sample).
#include <QtTest>
#include <QApplication>
#include <QSignalSpy>
#include "src/shell/ShellWindow.h"
#include "src/bridge/ContentBridge.h"

namespace {
// Count pixels close to Borland navy #0000aa = (0,0,170).
int countNavy(const QImage& img)
{
    int n = 0;
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x) {
            const QColor c = img.pixelColor(x, y);
            if (qAbs(c.red() - 0x1A) <= 6 && qAbs(c.green() - 0x1A) <= 6 &&
                qAbs(c.blue() - 0xAA) <= 8)
                ++n;
        }
    return n;
}
} // namespace

class TstBridge : public QObject
{
    Q_OBJECT
private slots:
    void frame_contains_shell_background() {
        ShellWindow w;
        w.resize(640, 480);
        ContentBridge b;
        b.setShellWindow(&w);
        b.update();
        const QImage f = b.frame();
        QVERIFY2(!f.isNull(), "frame is null after update()");
        QCOMPARE(f.size(), QSize(640, 480));
        const int navy = countNavy(f);
        QVERIFY2(navy > 10000,
                 qPrintable(QStringLiteral("navy px=%1, expected >10000").arg(navy)));
    }

    void update_emits_frameChanged() {
        ShellWindow w;
        w.resize(320, 200);
        ContentBridge b;
        b.setShellWindow(&w);
        QSignalSpy spy(&b, &ContentBridge::frameChanged);
        b.update();
        QCOMPARE(spy.count(), 1);
    }

    void sourceItem_property_exists() {
        ContentBridge b;
        QVERIFY(b.metaObject()->indexOfProperty("sourceItem") >= 0);
    }

    void image_provider_serves_frame() {
        ShellWindow w;
        w.resize(320, 200);
        ContentBridge b;
        b.setShellWindow(&w);
        b.update();
        BridgeImageProvider p(&b);
        QSize sz;
        const QImage img = p.requestImage(QStringLiteral("frame"), &sz, QSize());
        QVERIFY(!img.isNull());
        QCOMPARE(img.size(), QSize(320, 200));
        QVERIFY(countNavy(img) > 1000);
    }

    void resize_shell_follows_window() {
        ShellWindow w;
        w.resize(320, 200);
        ContentBridge b;
        b.setShellWindow(&w);
        b.resizeShell(800, 600);
        QCOMPARE(w.size(), QSize(800, 600));
    }
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    TstBridge tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "tst_bridge.moc"