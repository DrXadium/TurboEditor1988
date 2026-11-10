// SPDX-License-Identifier: GPL-3.0-or-later
// Task 9B: CRT'd popup menu contract test.
// Proves: registering an open QMenu with ContentBridge makes the menu's pixels
// appear in the composited frame (the menu is captured into the same texture the
// CRT samples — not left as a separate native window).
#include <QtTest>
#include <QApplication>
#include <QMenu>
#include <QMenuBar>
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

    // Task 10: Kate's option menus present in Borland chrome.
    void menu_bar_has_kate_option_menus() {
        ShellWindow w;
        QMenuBar* mb = w.menuBar();
        QVERIFY2(mb, "no menu bar");

        const QStringList kateMenus = { QStringLiteral("File"), QStringLiteral("Edit"),
                                        QStringLiteral("Search"), QStringLiteral("View"),
                                        QStringLiteral("Session"), QStringLiteral("Settings"),
                                        QStringLiteral("Help") };
        const QList<QAction*> topActions = mb->actions();
        QStringList titles;
        for (QAction* a : topActions)
            titles << a->text().remove(QLatin1Char('&'));   // strip hotkey marker

        for (const QString& want : kateMenus) {
            QVERIFY2(titles.contains(want),
                     qPrintable(QStringLiteral("menu bar missing Kate menu '%1' (have: %2)")
                                    .arg(want, titles.join(QLatin1Char(',')))));
        }
        // Each top-level menu must open a submenu with entries (not a dead action).
        for (QAction* a : topActions) {
            QVERIFY2(a->menu(), qPrintable(QStringLiteral("menu '%1' has no submenu")
                                               .arg(a->text())));
            QVERIFY2(a->menu()->actions().size() > 0,
                     qPrintable(QStringLiteral("menu '%1' is empty").arg(a->text())));
        }
    }

    // Task 10: menu entries the composition root wires to real features exist
    // under stable objectNames, and toggles are checkable.
    void menu_entries_expose_wiring_targets() {
        ShellWindow w;
        QMenuBar* mb = w.menuBar();
        QVERIFY(mb);

        const QStringList required = {
            QStringLiteral("actionFileNew"),   QStringLiteral("actionFileOpen"),
            QStringLiteral("actionFileSave"),  QStringLiteral("actionFileSaveAs"),
            QStringLiteral("actionEditUpper"), QStringLiteral("actionEditLower"),
            QStringLiteral("actionEditSort"),  QStringLiteral("actionEditSpellcheck"),
            QStringLiteral("actionSearchFind"),
            QStringLiteral("actionSearchFindNext"),
            QStringLiteral("actionViewZoomIn"), QStringLiteral("actionViewZoomOut"),
            QStringLiteral("actionViewZoomReset"),
            QStringLiteral("actionViewMinimap"), QStringLiteral("actionViewFoldBar"),
            QStringLiteral("actionSessionSave"), QStringLiteral("actionSessionRestore"),
            QStringLiteral("actionSettingsConfigure"),
            QStringLiteral("actionHelpAbout"),
        };
        for (const QString& name : required) {
            QVERIFY2(mb->findChild<QAction*>(name),
                     qPrintable(QStringLiteral("menu bar missing wiring target '%1'").arg(name)));
        }
        // Checkable toggles.
        for (const QString& name : { QStringLiteral("actionViewMinimap"),
                                     QStringLiteral("actionViewFoldBar"),
                                     QStringLiteral("actionEditSpellcheck") }) {
            QAction* a = mb->findChild<QAction*>(name);
            QVERIFY2(a && a->isCheckable(),
                     qPrintable(QStringLiteral("'%1' should be a checkable toggle").arg(name)));
        }
    }

    // Task 10: Borland palette + hotkey highlight applied to the menu bar items.
    void menu_bar_uses_borland_palette_with_hotkey_highlight() {
        ShellWindow w;
        QMenuBar* mb = w.menuBar();
        QVERIFY(mb);

        // Borland navy desktop is #0000aa (170 blue); hotkey highlight #ffff55.
        const QPalette pal = mb->palette();
        const QColor navBg = pal.color(QPalette::Active, QPalette::Button);
        QVERIFY2(qAbs(navBg.blue() - 170) < 24 && navBg.red() < 60 && navBg.green() < 60,
                 qPrintable(QStringLiteral("menu bar background is not Borland navy: #%1%2%3")
                                .arg(navBg.red(), 2, 16, QLatin1Char('0'))
                                .arg(navBg.green(), 2, 16, QLatin1Char('0'))
                                .arg(navBg.blue(), 2, 16, QLatin1Char('0'))));
        const QColor hl = pal.color(QPalette::Active, QPalette::HighlightedText);
        QVERIFY2(hl.red() > 200 && hl.green() > 200 && hl.blue() < 130,
                 qPrintable(QStringLiteral("hotkey highlight is not Borland yellow: #%1%2%3")
                                .arg(hl.red(), 2, 16, QLatin1Char('0'))
                                .arg(hl.green(), 2, 16, QLatin1Char('0'))
                                .arg(hl.blue(), 2, 16, QLatin1Char('0'))));
    }
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    TstMenus tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "tst_menus.moc"