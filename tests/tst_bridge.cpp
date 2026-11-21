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
#include "src/shell/TvPlate.h"
#include "src/editor/EditorTabManager.h"
#include "src/editor/FindReplaceDialog.h"
#include <QPushButton>
#include <QLineEdit>
#include <ktexteditor/document.h>

namespace {
// Count pixels close to Borland navy #0000aa = (0,0,170).
int countNavy(const QImage& img)
{
    int n = 0;
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x) {
            const QColor c = img.pixelColor(x, y);
            if (qAbs(c.red() - 0x00) <= 6 && qAbs(c.green() - 0x00) <= 6 &&
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

    // Reported live: "buttons don't return to an unpressed state / color if
    // released". The widget-level press/release pair passes while the real app
    // sticks, so the difference is the SYNTHESIZED events the bridge delivers:
    // QML's MouseArea forwards `mouse.buttons`, which on a Qt release is
    // NoButton, and `QAbstractButton` ignores a release whose button is not
    // LeftButton - leaving `down` true and the plate bright/shifted forever.
    // This test drives a real TvPlate through forwardMouseEvent exactly as the
    // QML does.
    void a_release_through_the_bridge_returns_the_plate_to_rest()
    {
        ShellWindow shell;
        shell.resize(640, 400);
        shell.show();
        QCoreApplication::processEvents();

        ContentBridge bridge;
        bridge.setShellWindow(&shell);

        // A plate parented to the shell, in shell coordinates the bridge can
        // resolve with childAt().
        auto* plate = new TvPlate(QStringLiteral("OK"), &shell);
        plate->setGeometry(200, 150, 120, 40);
        plate->show();
        QCoreApplication::processEvents();

        const QPoint center = plate->mapTo(&shell, plate->rect().center());

        // PRESS with buttons = LeftButton (QML's onPressed: mouse.buttons).
        bridge.forwardMouseEvent(QEvent::MouseButtonPress, Qt::LeftButton,
                                 Qt::LeftButton, Qt::NoModifier,
                                 center.x(), center.y());
        QCoreApplication::processEvents();
        QVERIFY2(plate->isDown(), "the press did not land on the plate");

        // RELEASE exactly as QML's onReleased does: mouse.button = LeftButton,
        // mouse.buttons = NoButton (Qt reports no buttons held on release).
        bridge.forwardMouseEvent(QEvent::MouseButtonRelease, Qt::LeftButton,
                                 Qt::NoButton, Qt::NoModifier,
                                 center.x(), center.y());
        QCoreApplication::processEvents();

        QVERIFY2(!plate->isDown(),
                 "the plate is still pressed after a release through the "
                 "bridge - it will paint bright and shifted forever");
    }

    // Reported live: "i cannot type in the path bar; clicking in to it does
    // nothing". The same must not happen to the Find dialog: a click routed
    // through the bridge has to land in the field and focus it, and the keys
    // that follow have to arrive there.
    void a_click_and_typing_reach_the_find_field()
    {
        ShellWindow shell;
        shell.resize(760, 520);
        shell.show();
        QCoreApplication::processEvents();

        ContentBridge bridge;
        bridge.setShellWindow(&shell);

        EditorTabManager tabs;
        tabs.newDocument();
        tabs.activeDocument()->setText(QStringLiteral("needle"));

        auto* dlg = new FindReplaceDialog(&tabs, &shell);
        dlg->setGeometry(20, 20, dlg->sizeHint().width(), dlg->sizeHint().height());
        dlg->show();
        QCoreApplication::processEvents();

        QLineEdit* field = dlg->findField();
        QVERIFY(field);
        field->setText(QString());
        const QPoint centre = field->mapTo(&shell, field->rect().center());

        bridge.forwardMouseEvent(QEvent::MouseButtonPress, Qt::LeftButton, Qt::LeftButton,
                                 Qt::NoModifier, centre.x(), centre.y());
        bridge.forwardMouseEvent(QEvent::MouseButtonRelease, Qt::LeftButton, Qt::NoButton,
                                 Qt::NoModifier, centre.x(), centre.y());
        QCoreApplication::processEvents();

        QVERIFY2(field->hasFocus(), "the click through the bridge did not focus the find field");

        for (const char c : {'a', 'b', 'c'}) {
            bridge.forwardKeyEvent(QEvent::KeyPress, Qt::Key_A + (c - 'a'), Qt::NoModifier,
                                   QString(QChar(c)));
        }
        QCoreApplication::processEvents();
        QCOMPARE(field->text(), QStringLiteral("abc"));
    }

    // Reported 2026-11-21: "double clicking on .. doesn't move you up a
    // directory". QML's MouseArea emits only press/release pairs - Qt's own
    // MouseButtonDblClick synthesis never runs for a synthesized event, so the
    // dock never saw the double-click and descendInto() was dead code. The
    // bridge must synthesize it: the second press on the same spot within the
    // double-click interval becomes DblClick (Qt's own rule), routed exactly
    // like a press.
    void a_second_bridge_press_synthesizes_a_double_click() {
        ShellWindow shell;
        shell.resize(800, 600);
        shell.show();
        QCoreApplication::processEvents();

        ContentBridge bridge;
        bridge.setShellWindow(&shell);

        // A plain widget that counts DblClick deliveries (the real dock's
        // handler runs descendInto() off the same event type).
        auto* probe = new QWidget(&shell);
        probe->setGeometry(40, 40, 120, 60);
        probe->show();
        int dblClicks = 0;
        class DblProbe : public QObject
        {
        public:
            int* n;
            bool eventFilter(QObject*, QEvent* ev) override
            {
                if (ev->type() == QEvent::MouseButtonDblClick)
                    ++*n;
                return false;
            }
        };
        auto* pr = new DblProbe;
        pr->n = &dblClicks;
        probe->installEventFilter(pr);

        const int x = 100, y = 70;
        // Release BETWEEN the two presses (QML sends a full click pair), so the
        // probe must also prove a stale release does NOT re-arm the pairing -
        // the mutation "skip pairing on release" otherwise hides behind it.
        auto click = [&bridge, x, y] {
            bridge.forwardMouseEvent(QEvent::MouseButtonPress, Qt::LeftButton,
                                     Qt::LeftButton, Qt::NoModifier, x, y);
            bridge.forwardMouseEvent(QEvent::MouseButtonRelease, Qt::LeftButton,
                                     Qt::NoButton, Qt::NoModifier, x, y);
        };
        click();
        QCoreApplication::processEvents();
        QVERIFY2(dblClicks == 0, "a single click synthesized a double-click");

        // Second press at the same coordinates immediately: double-click.
        click();
        QCoreApplication::processEvents();
        QVERIFY2(dblClicks == 1,
                 qPrintable(QStringLiteral("the second press did not become a "
                                           "double-click (saw %1)").arg(dblClicks)));

        // A press far away in time/space must not double-click: the bridge
        // only pairs presses on the SAME spot within the interval.
        QTest::qWait(400);   // beyond Qt's 400 ms double-click interval
        click();
        QCoreApplication::processEvents();
        QVERIFY2(dblClicks == 1,
                 qPrintable(QStringLiteral("a stale press still counted as a "
                                           "double-click (%1)").arg(dblClicks)));
    }
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    TstBridge tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "tst_bridge.moc"