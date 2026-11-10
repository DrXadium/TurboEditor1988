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

    // Task 12 (Turbo Vision spec): menu bar = light-gray #AAAAAA bg, BLACK
    // label text; window blue = #1A1AAA; hotkey = red #C41E1E (a QAction's
    // highlighted role doubles as the hotkey letter in this theme).
    // Task 3: canonical Turbo Vision placement — the first top-level title
    // starts at ~column 2 (2 cell widths from the left) and consecutive titles
    // are separated by a small, consistent gap (TV's fixed-column layout), not
    // QMenuBar's default flush packing.
    void menu_items_use_tv_column_placement() {
        ShellWindow w;
        w.resize(800, 600);
        w.show();
        QMenuBar* mb = w.menuBar();
        QVERIFY(mb);
        const QList<QAction*> tops = mb->actions();
        QVERIFY(tops.size() >= 2);

        const QFontMetrics fm(mb->font());
        const int cellW = fm.horizontalAdvance(QLatin1Char('M'));   // one DOS cell
        const QRect g0 = mb->actionGeometry(tops.at(0));

        // First item starts at ~column 2 (allow a little style padding slack).
        QVERIFY2(g0.x() >= cellW && g0.x() <= 4 * cellW,
                 qPrintable(QStringLiteral(
                     "first menu item starts at x=%1, expected ~column 2 (%2..%3 px)")
                                .arg(g0.x()).arg(2 * cellW).arg(4 * cellW)));

        // Consecutive items are separated by a small, consistent gap (TV uses a
        // fixed inter-title space; QMenuBar default packs them tighter/looser).
        const QRect g1 = mb->actionGeometry(tops.at(1));
        const int gap = g1.x() - g0.right();
        QVERIFY2(gap > 0 && gap <= 3 * cellW,
                 qPrintable(QStringLiteral(
                     "menu gap between items = %1 px, expected a small TV gap (1..%2)")
                                .arg(gap).arg(3 * cellW)));
    }

    void menu_bar_uses_turbo_vision_palette() {
        ShellWindow w;
        QMenuBar* mb = w.menuBar();
        QVERIFY(mb);

        const QPalette pal = mb->palette();
        const QColor barBg = pal.color(QPalette::Active, QPalette::Button);
        QVERIFY2(qAbs(barBg.red() - 0xAA) < 24 && qAbs(barBg.green() - 0xAA) < 24 &&
                 qAbs(barBg.blue() - 0xAA) < 24,
                 qPrintable(QStringLiteral("menu bar background is not TV gray #AAAAAA: #%1%2%3")
                                .arg(barBg.red(), 2, 16, QLatin1Char('0'))
                                .arg(barBg.green(), 2, 16, QLatin1Char('0'))
                                .arg(barBg.blue(), 2, 16, QLatin1Char('0'))));
        const QColor txt = pal.color(QPalette::Active, QPalette::ButtonText);
        QVERIFY2(txt.red() < 40 && txt.green() < 40 && txt.blue() < 40,
                 qPrintable(QStringLiteral("menu bar label is not black: #%1%2%3")
                                .arg(txt.red(), 2, 16, QLatin1Char('0'))
                                .arg(txt.green(), 2, 16, QLatin1Char('0'))
                                .arg(txt.blue(), 2, 16, QLatin1Char('0'))));
        const QColor hl = pal.color(QPalette::Active, QPalette::HighlightedText);
        QVERIFY2(hl.red() > 0x9E && hl.green() < 0x50 && hl.blue() < 0x50,
                 qPrintable(QStringLiteral("hotkey is not TV red #C41E1E: #%1%2%3")
                                .arg(hl.red(), 2, 16, QLatin1Char('0'))
                                .arg(hl.green(), 2, 16, QLatin1Char('0'))
                                .arg(hl.blue(), 2, 16, QLatin1Char('0'))));
    }

    // Task 12: window/desktop blue is TV #1A1AAA (not old navy #0000aa),
    // verified on real painted pixels via ContentBridge frame.
    void window_surface_is_tv_blue() {
        ShellWindow w;
        w.resize(320, 240);
        ContentBridge b;
        b.setShellWindow(&w);
        b.update();
        const QImage f = b.frame();
        QVERIFY(!f.isNull());
        int blue = 0;
        for (int y = 0; y < f.height(); ++y)
            for (int x = 0; x < f.width(); ++x) {
                const QColor c = f.pixelColor(x, y);
                if (qAbs(c.red() - 0x1A) <= 8 && qAbs(c.green() - 0x1A) <= 8 &&
                    qAbs(c.blue() - 0xAA) <= 10)
                    ++blue;
            }
        QVERIFY2(blue > 1000,
                 qPrintable(QStringLiteral("TV-blue #1A1AAA px=%1, expected >1000").arg(blue)));
    }
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    TstMenus tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "tst_menus.moc"