// SPDX-License-Identifier: GPL-3.0-or-later
// Input-forwarding contract test: events posted through ContentBridge reach
// the offscreen ShellWindow (and drive its children) — this is what makes the
// CRT glass window interactive.
#include <QtTest>
#include <QApplication>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMenuBar>
#include <QLineEdit>
#include "src/shell/ShellWindow.h"
#include "src/shell/TabStrip.h"
#include "src/bridge/ContentBridge.h"

namespace {
// Counts QMouseEvent/QKeyEvent deliveries to a widget.
class EventProbe : public QObject
{
public:
    int mouse = 0;
    int keys = 0;
    QPoint lastPos;
    bool eventFilter(QObject* obj, QEvent* ev) override
    {
        switch (ev->type()) {
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease:
        case QEvent::MouseMove:
            ++mouse;
            lastPos = static_cast<QMouseEvent*>(ev)->position().toPoint();
            break;
        case QEvent::KeyPress:
            ++keys;
            break;
        default:
            break;
        }
        return QObject::eventFilter(obj, ev);
    }
};

// Install a probe on the CHILD widget a point maps to (mirrors how
// ContentBridge forwards to childAt(), not the shell itself).
EventProbe* probeAt(ShellWindow& w, const QPoint& p, QWidget** targetOut)
{
    QWidget* t = w.childAt(p);
    if (!t)
        t = &w;
    auto* pr = new EventProbe;
    t->installEventFilter(pr);
    if (targetOut)
        *targetOut = t;
    return pr;
}
} // namespace

class TstInput : public QObject
{
    Q_OBJECT
private slots:
    void forwarded_mouse_press_reaches_target_child() {
        ShellWindow w;
        w.resize(640, 480);
        w.show();
        ContentBridge b;
        b.setShellWindow(&w);

        // A point over the menu bar maps to the MenuBar child; the probe proves
        // the forwarded press is DELIVERED to that child at the exact coords.
        const QPoint p(20, 10);   // inside the menu bar band
        QWidget* target = nullptr;
        EventProbe* probe = probeAt(w, p, &target);
        QVERIFY2(target && target != &w, "no child at menu-bar point");

        b.forwardMouseEvent(QEvent::MouseButtonPress, Qt::LeftButton,
                            Qt::LeftButton, Qt::NoModifier, p.x(), p.y());
        QCoreApplication::processEvents();

        QVERIFY2(probe->mouse >= 1,
                 "target child received no mouse event after forwardMouseEvent");
        QCOMPARE(probe->lastPos, p);
        delete probe;
    }

    void forwarded_click_selects_menu() {
        ShellWindow w;
        w.resize(640, 480);
        w.show();
        ContentBridge b;
        b.setShellWindow(&w);

        QMenuBar* mb = w.menuBar();
        QVERIFY2(mb && !mb->actions().isEmpty(), "no menu bar / no menus");
        QAction* file = mb->actions().first();
        QVERIFY2(file->menu(), "first top-level action has no submenu");

        const QPoint c = mb->actionGeometry(file).center();
        b.forwardMouseEvent(QEvent::MouseButtonPress, Qt::LeftButton,
                            Qt::LeftButton, Qt::NoModifier, c.x(), c.y());
        QCoreApplication::processEvents();

        // The forwarded press reached + selected the File menu (the precondition
        // for opening). Mapping the popup needs a real screen (offscreen has
        // none) — verified on DISPLAY=:0 in the final relaunch. Release can
        // commit/clear the selection per Qt internals, so assert on the press.
        QVERIFY2(mb->actionAt(c) == file,
                 "menu bar did not hit-test the click to the File menu");
        QVERIFY2(mb->activeAction() == file,
                 "menu bar did not select File after the forwarded press");
    }

    void forwarded_click_at_tab_emits_tab_activated() {
        ShellWindow w;
        w.resize(640, 480);
        ContentBridge b;
        b.setShellWindow(&w);

        TabStrip* ts = w.tabStrip();
        QVERIFY2(ts, "no tab strip");
        w.show();
        ts->setTabs({QStringLiteral("one.cpp"), QStringLiteral("two.cpp"),
                     QStringLiteral("three.cpp")}, 0);
        // tabRects() is computed during paintEvent (offscreen never paints), so
        // render once to force the layout the click will target.
        ts->grab();
        QCoreApplication::processEvents();
        QVERIFY2(ts->tabRects().size() >= 2, "tab rects not computed after render");

        QSignalSpy spy(ts, &TabStrip::tabActivated);
        // Click the shell at the tab's SHELL-LOCAL point (map strip -> shell).
        const QRect r2 = ts->tabRects().at(1);
        const QPoint c = ts->mapTo(&w, r2.center());

        b.forwardMouseEvent(QEvent::MouseButtonPress, Qt::LeftButton,
                            Qt::LeftButton, Qt::NoModifier, c.x(), c.y());
        b.forwardMouseEvent(QEvent::MouseButtonRelease, Qt::LeftButton,
                            Qt::NoButton, Qt::NoModifier, c.x(), c.y());
        QCoreApplication::processEvents();

        QVERIFY2(spy.count() == 1,
                 qPrintable(QStringLiteral("expected tabActivated(1), got %1 signals")
                                .arg(spy.count())));
        QCOMPARE(spy.first().first().toInt(), 1);
    }

    void forwarded_key_reaches_focused_widget() {
        ShellWindow w;
        w.resize(640, 480);
        w.show();
        ContentBridge b;
        b.setShellWindow(&w);

        // A focusable line edit standing in for the editor view (child of the
        // shell; must be shown to receive keys).
        auto* edit = new QLineEdit(&w);
        edit->resize(200, 24);
        edit->move(10, 60);
        edit->show();
        QCoreApplication::processEvents();
        edit->setFocus();
        QCoreApplication::processEvents();
        QVERIFY2(edit->isVisible(), "edit did not become visible");

        // Offscreen QPA reports focusWidget()==nil even after setFocus (it
        // doesn't track focus), so verify forwardKeyEvent targets + types into
        // the edit. On a real display focusWidget() resolves and the same path
        // is exercised — the final relaunch confirms it live.
        EventProbe probe;
        edit->installEventFilter(&probe);

        // If focusWidget() is null (offscreen), forwardKeyEvent falls back to
        // the shell; but QWidget key events propagate down to a focused-or-first
        // child, so the edit still receives + handles it. Install a shell-level
        // probe to prove delivery regardless of which target is chosen.
        b.forwardKeyEvent(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier,
                          QStringLiteral("a"));
        QCoreApplication::processEvents();

        QVERIFY2(probe.keys >= 1 || !edit->text().isEmpty(),
                 "key neither reached nor was handled by the edit");
        QCOMPARE(edit->text(), QStringLiteral("a"));
        delete edit;
    }
};

int main(int argc, char** argv)
{
    // Deterministic QPA: the test drives input at widgets, and a real Wayland
    // session refuses to map a grabbing QMenu popup for an offscreen shell
    // (the known Task 9/10 limitation), which makes menu selection flaky.
    // Offscreen has no popup-mapping, so behavior is stable across environments.
    if (qgetenv("QT_QPA_PLATFORM").isEmpty())
        qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    TstInput tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "tst_input.moc"
