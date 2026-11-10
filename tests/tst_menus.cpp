// SPDX-License-Identifier: GPL-3.0-or-later
// Task 9B: CRT'd popup menu contract test.
// Proves: registering an open QMenu with ContentBridge makes the menu's pixels
// appear in the composited frame (the menu is captured into the same texture the
// CRT samples — not left as a separate native window).
#include <QtTest>
#include <QApplication>
#include <QMenu>
#include <QImage>
#include <QGuiApplication>
#include "src/shell/ShellWindow.h"
#include "src/bridge/ContentBridge.h"

class TstMenus : public QObject
{
    Q_OBJECT
private slots:
    void registered_popup_appears_in_composited_frame() {
        ShellWindow w;
        w.resize(640, 480);
        ContentBridge b;
        b.setShellWindow(&w);
        b.update();
        const QImage before = b.frame();

        // An open popup floating over the shell's top-left area.
        QMenu menu;
        menu.addAction(QStringLiteral("Open File"));
        menu.addAction(QStringLiteral("Save"));
        menu.resize(160, 80);
        menu.move(w.pos().x() + 40, w.pos().y() + 60);
        b.registerPopup(&menu);
        menu.show();
        QVERIFY(menu.isVisible());
        QApplication::processEvents();

        b.update();
        const QImage after = b.frame();
        QVERIFY(!after.isNull());

        int changed = 0;
        const QPoint popupTopLeft = menu.pos() - w.pos();
        const QRect region(popupTopLeft, menu.size());
        QVERIFY(region.right() < after.width() && region.bottom() < after.height());
        for (int y = region.top(); y <= region.bottom(); ++y)
            for (int x = region.left(); x <= region.right(); ++x)
                if (after.pixelColor(x, y) != before.pixelColor(x, y))
                    ++changed;
        QVERIFY2(changed > 500,
                 qPrintable(QStringLiteral("popup region barely changed: %1 px").arg(changed)));

        menu.hide();
        b.unregisterPopup(&menu);
    }

    // Root-cause regression (Wayland): a QMenu fails to map as a grabbing popup
    // when its transient parent has no native surface, so isVisible() reverts to
    // false after event dispatch and the bridge's visibility guard skips it.
    // registerPopup must leave the popup in a state that stays visible + renders
    // (spec's non-native-menu fallback), regardless of platform.
    void registered_popup_stays_visible_after_event_dispatch() {
        ShellWindow w;
        w.resize(640, 480);
        ContentBridge b;
        b.setShellWindow(&w);

        QMenu menu;
        menu.addAction(QStringLiteral("Open File"));
        menu.resize(160, 80);
        menu.move(w.pos().x() + 40, w.pos().y() + 60);
        b.registerPopup(&menu);   // must harden the popup for compositing
        menu.show();
        QApplication::processEvents();

        QVERIFY2(menu.isVisible(),
                 qPrintable(QStringLiteral("popup did not stay visible after event dispatch "
                                           "(platform=%1) - bridge would skip compositing it")
                                .arg(QGuiApplication::platformName())));
        b.unregisterPopup(&menu);
    }
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    TstMenus tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "tst_menus.moc"