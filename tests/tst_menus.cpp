// SPDX-License-Identifier: GPL-3.0-or-later
// Task 9B: CRT'd popup menu contract test.
// Proves: registering an open QMenu with ContentBridge makes the menu's pixels
// appear in the composited frame (the menu is captured into the same texture the
// CRT samples — not left as a separate native window).
#include <QtTest>
#include <QApplication>
#include <QMenu>
#include <QMenuBar>
#include <QStyle>
#include <QImage>
#include <QGuiApplication>
#include <QPainter>
#include <QStyleOptionMenuItem>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QMouseEvent>
#include "src/shell/ShellWindow.h"
#include "src/shell/BorlandStyle.h"
#include "src/shell/MenuBar.h"
#include "src/shell/BorlandMenu.h"
#include "src/shell/TabStrip.h"
#include "src/shell/TabListMenu.h"
#include "src/shell/FKeyStatusBar.h"
#include "src/bridge/ContentBridge.h"
#include "src/editor/EditorTabManager.h"
#include <ktexteditor/document.h>
#include <ktexteditor/view.h>

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
    // Task 21 (mock note 5): ▼ opens a dropdown of every open tab, styled
    // exactly like the menu-bar dropdowns (a BorlandMenu), with the active tab
    // checked and each entry carrying its tab index.
    // Task 26 (user): the ✓ of a checked item COLLIDED with its label - it was
    // drawn at the panel's left edge, over the text. It belongs at the RIGHT of
    // the item, one cell in from the end, with the label's column left clean.
    void check_mark_sits_at_the_right_end_of_the_item() {
        BorlandMenu menu;
        QAction* checked = menu.addAction(QStringLiteral("alpha"));
        checked->setCheckable(true);
        checked->setChecked(true);
        QAction* plain = menu.addAction(QStringLiteral("beta"));
        plain->setCheckable(true);
        menu.adjustSize();
        menu.show();
        QCoreApplication::processEvents();

        const QImage img = menu.grab().toImage().convertToFormat(QImage::Format_RGB32);
        QVERIFY(!img.isNull());
        const QFontMetrics fm = menu.fontMetrics();
        const int cell = qMax(1, fm.horizontalAdvance(QLatin1Char('M')));
        const int contentLeft = BorlandMenu::borderInset() + BorlandMenu::borderWidth();
        const int contentRight = img.width() - BorlandMenu::borderInset()
                               - BorlandMenu::borderWidth() - 1;
        const auto isInk = [](const QColor& c) {
            return c.red() < 60 && c.green() < 60 && c.blue() < 60;
        };

        const QRect row = menu.actionGeometry(checked);
        QVERIFY2(row.isValid() && row.height() > 4, "no item geometry");

        int leftInk = 0, rightInk = 0, rightmost = -1;
        for (int y = row.top(); y <= row.bottom() && y < img.height(); ++y)
            // Content area only: the black border stroke at each edge is not
            // item ink and must not be mistaken for the check.
            for (int x = contentLeft; x <= contentRight && x < img.width(); ++x) {
                if (!isInk(img.pixelColor(x, y)))
                    continue;
                if (x < contentLeft + cell)
                    ++leftInk;                  // the label's own column
                if (x > contentRight - 3 * cell) {
                    ++rightInk;
                    rightmost = qMax(rightmost, x);
                }
            }

        QVERIFY2(leftInk == 0,
                 qPrintable(QStringLiteral("%1 ink px in the label column - the check "
                                           "still sits over the text").arg(leftInk)));
        QVERIFY2(rightInk > 3,
                 qPrintable(QStringLiteral("no check at the right end (%1 ink px)")
                                .arg(rightInk)));
        QVERIFY2(rightmost <= contentRight - cell,
                 qPrintable(QStringLiteral("the check runs into the item's end: rightmost "
                                           "ink at %1, must stop by %2")
                                .arg(rightmost).arg(contentRight - cell)));
    }

    // Task 22 (reported live): the dropdown opened OFF THE WINDOW. Anchored at
    // the ▼ cell's LEFT edge, a 163 px list on a 1280 px window hung 155 px past
    // the right edge, so the compositor could only ever show a sliver - it read
    // as "clicking ▼ does nothing". It must right-align to the control and stay
    // inside the shell.
    void tab_list_opens_inside_the_window() {
        const QRect shell(0, 0, 1280, 800);
        const QSize menu(163, 104);
        // The ▼ control is the window's last three cells: x 1272..1279.
        const QRect cell(1272, 25, 8, 16);
        const QPoint at = TabListMenu::anchoredPos(cell, menu, shell);
        QVERIFY2(at.x() >= shell.left(), "list runs off the left edge");
        QVERIFY2(at.x() + menu.width() <= shell.left() + shell.width(),
                 qPrintable(QStringLiteral("list runs off the right edge: x=%1 w=%2")
                                .arg(at.x()).arg(menu.width())));
        QCOMPARE(at.x() + menu.width(), cell.right() + 1);   // right-aligned
        QCOMPARE(at.y(), cell.bottom() + 1);                 // below the row
        // A control in the MIDDLE of the row must still open below it.
        const QRect mid(400, 25, 8, 16);
        const QPoint midAt = TabListMenu::anchoredPos(mid, menu, shell);
        QCOMPARE(midAt.y(), mid.bottom() + 1);
        QVERIFY2(midAt.x() + menu.width() <= shell.left() + shell.width(), "clamped");
        // A list taller than the room left is pinned inside, not left hanging.
        const QPoint tall = TabListMenu::anchoredPos(QRect(400, 780, 8, 16),
                                                     QSize(163, 104), shell);
        QVERIFY2(tall.y() + 104 <= shell.top() + shell.height(),
                 qPrintable(QStringLiteral("list hangs off the bottom: y=%1").arg(tall.y())));
    }

    // Regression (reported live): opening the ▼ tab list RE-FLows the shell -
    // the editor body and its scrollbar climb over the document border row and
    // erase the frame's top-right corner. Cause: the menu was parented to the
    // TabStrip, which lives INSIDE the editor host's layout, so showing it
    // (WA_DontShowOnScreen, as the popup registrar does) disturbed that layout.
    void tab_list_popup_does_not_reflow_the_shell() {
        ShellWindow w;
        w.resize(640, 400);
        ContentBridge b;
        b.setShellWindow(&w);
        w.show();
        QCoreApplication::processEvents();

        TabStrip* strip = w.tabStrip();
        auto* titleRow = w.findChild<QWidget*>(QStringLiteral("editorTitleRow"));
        auto* bar = w.findChild<QWidget*>(QStringLiteral("rightScrollBar"));
        QVERIFY2(strip && titleRow && bar, "strip, border row or scrollbar missing");

        const QRect stripBefore = strip->geometry();
        const QRect titleBefore = titleRow->geometry();
        const QRect barBefore = bar->geometry();
        const QRect menuBefore = w.menuBar()->geometry();

        QMenu* menu = TabListMenu::build(strip, QStringList() << "alpha" << "beta", 0);
        // Exactly the path the app's PopupRegistrar takes when the menu shows.
        b.registerPopup(menu);
        QCoreApplication::processEvents();
        QVERIFY(menu->isVisible());

        // The menu must SURFACE as a popup: hand it the cursor grab the way
        // QMenu::exec does. Without it the list is never shown to the user and
        // the window manager never sees a popup there either.
        menu->grabMouse();
        menu->setFocus(Qt::PopupFocusReason);
        QVERIFY2(menu->isVisible() && menu->testAttribute(Qt::WA_DontShowOnScreen),
                 "tab list did not surface as a non-native popup");

        QVERIFY2(strip->geometry() == stripBefore,
                 qPrintable(QStringLiteral("tab row moved: %1 -> %2")
                                .arg(QString("%1,%2 %3x%4")
                                         .arg(stripBefore.x()).arg(stripBefore.y())
                                         .arg(stripBefore.width()).arg(stripBefore.height()),
                                     QString("%1,%2 %3x%4")
                                         .arg(strip->geometry().x()).arg(strip->geometry().y())
                                         .arg(strip->geometry().width()).arg(strip->geometry().height()))));
        QVERIFY2(titleRow->geometry() == titleBefore,
                 "document border row moved - the frame's corner row is displaced");
        QVERIFY2(bar->geometry() == barBefore,
                 "right scrollbar moved - it now covers the border row's corner");
        QVERIFY2(w.menuBar()->geometry() == menuBefore, "menu bar moved");

        delete menu;
    }

    void tab_list_dropdown_lists_every_tab_like_a_borland_menu() {
        const QStringList titles = QStringList() << "alpha" << "beta" << "gamma";
        QMenu* menu = TabListMenu::build(nullptr, titles, 1);
        QVERIFY2(menu, "no menu built");
        QVERIFY2(qobject_cast<BorlandMenu*>(menu),
                 "the tab list must use the menu-bar dropdown class, not a bare QMenu");
        const QList<QAction*> acts = menu->actions();
        QCOMPARE(acts.size(), 3);
        for (int i = 0; i < 3; ++i) {
            QCOMPARE(acts[i]->text(), titles.at(i));
            QCOMPARE(acts[i]->data().toInt(), i);
            QVERIFY2(acts[i]->isCheckable(), "tab entries must show the active one");
            QCOMPARE(acts[i]->isChecked(), i == 1);
        }
        delete menu;
    }

    // Xadium 2026-11-23: "let's rewire the main menu so ALT-F brings up the
    // File menu". Qt's own Alt-mnemonic path lives in QMenuBar's key handling
    // and needs an ACTIVE window; this shell is WA_DontShowOnScreen and never
    // becomes one, so the bar exposes the popup as a callable step that the
    // bridge's keyboard routing invokes.
    void alt_mnemonic_opens_the_matching_top_level_menu() {
        ShellWindow w;
        w.resize(800, 600);
        // Popups resolve screen() through the window handle; production's
        // ContentBridge forces it with winId() (see setShellWindow). Do the
        // same here or QMenu::popup dereferences a null QWindow.
        w.show();
        w.winId();
        QVERIFY(w.menuBar());
        auto* bar = qobject_cast<MenuBar*>(w.menuBar());
        QVERIFY2(bar, "shell menu bar is not a MenuBar");

        QVERIFY2(bar->openMenuByMnemonic(QLatin1Char('F')),
                 "Alt+F did not open a top-level menu");
        QCoreApplication::processEvents();

        QMenu* file = bar->menu(QStringLiteral("File"));
        QVERIFY2(file, "no File menu");
        QVERIFY2(file->isVisible(), "File menu was not popped up by its mnemonic");

        file->close();
        QCoreApplication::processEvents();
        QVERIFY2(!file->isVisible(), "File menu did not close");

        // An unused letter must not open anything (and must not crash).
        QVERIFY2(!bar->openMenuByMnemonic(QLatin1Char('Q')),
                 "an unmapped letter opened a menu");
    }

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

    // 2026-11-26 (user request): File > Exit is the menu's LAST row and quits
    // through the normal path (aboutToQuit then persists session + settings).
    // User request 2026-11-28: File gains a "File Operations" submenu holding
    // Copy file location / Open containing folder, fenced off from the other
    // entries by separators, with Exit still the last row of File.
    void file_menu_offers_a_file_operations_submenu_before_exit() {
        ShellWindow w;
        auto* bar = qobject_cast<MenuBar*>(w.menuBar());
        QVERIFY2(bar, "shell menu bar is not a MenuBar");
        QMenu* file = bar->menu(QStringLiteral("File"));
        QVERIFY2(file, "no File menu");

        QAction* entry = bar->findChild<QAction*>(QStringLiteral("actionFileOperations"));
        QVERIFY2(entry, "File menu has no File Operations submenu");
        QVERIFY2(entry->text().remove(QLatin1Char('&')).startsWith(QLatin1String("File Operations")),
                 qPrintable(QStringLiteral("submenu text is '%1'").arg(entry->text())));

        QMenu* ops = entry->menu();
        QVERIFY2(ops, "File Operations carries no menu");

        QAction* copy = ops->findChild<QAction*>(QStringLiteral("actionFileCopyLocation"));
        QVERIFY2(copy, "no Copy file location action");
        QAction* folder = ops->findChild<QAction*>(QStringLiteral("actionFileOpenContainingFolder"));
        QVERIFY2(folder, "no Open containing folder action");
        QVERIFY2(ops->actions().contains(copy) && ops->actions().contains(folder),
                 "the two operations are not inside the submenu");

        // Fenced off from the rows above, and Exit still closes the menu.
        const QList<QAction*> rows = file->actions();
        const int at = rows.indexOf(entry);
        QVERIFY(at > 0);
        QVERIFY2(rows.at(at - 1)->isSeparator(),
                 "File Operations is not preceded by a separator");
        QAction* exit = bar->findChild<QAction*>(QStringLiteral("actionFileExit"));
        QVERIFY(exit);
        QCOMPARE(rows.last(), exit);
        QVERIFY(at < rows.indexOf(exit));

        // Nothing is open in a fresh shell, so both rows start disabled.
        QVERIFY2(!copy->isEnabled(), "Copy file location should start disabled");
        QVERIFY2(!folder->isEnabled(), "Open containing folder should start disabled");
    }

    void file_menu_offers_exit_as_its_last_row() {
        ShellWindow w;
        auto* bar = qobject_cast<MenuBar*>(w.menuBar());
        QVERIFY2(bar, "shell menu bar is not a MenuBar");
        QMenu* file = bar->menu(QStringLiteral("File"));
        QVERIFY2(file, "no File menu");

        QAction* exit = bar->findChild<QAction*>(QStringLiteral("actionFileExit"));
        QVERIFY2(exit, "File menu has no Exit action");
        QVERIFY2(exit->text().remove(QLatin1Char('&')).startsWith(QLatin1String("Exit")),
                 qPrintable(QStringLiteral("Exit action text is '%1'").arg(exit->text())));
        QCOMPARE(file->actions().last(), exit);
    }

    // Undo / Redo belong in the Edit menu above the clipboard block, with the
    // conventional shortcuts, and start DISABLED - a fresh buffer has no
    // history, so offering an enabled Undo would be a lie.
    void edit_menu_offers_disabled_undo_and_redo() {
        ShellWindow w;
        QMenuBar* mb = w.menuBar();
        QVERIFY(mb);
        QAction* undo = mb->findChild<QAction*>(QStringLiteral("actionEditUndo"));
        QAction* redo = mb->findChild<QAction*>(QStringLiteral("actionEditRedo"));
        QVERIFY2(undo, "Edit menu has no Undo entry");
        QVERIFY2(redo, "Edit menu has no Redo entry");

        // Ctrl+Z / Ctrl+Y as the user specified. (Qt's QKeySequence::Redo is
        // Ctrl+Shift+Z on Linux, so that enum is deliberately not asserted.)
        QCOMPARE(undo->shortcut(), QKeySequence(QKeySequence::Undo));
        QCOMPARE(undo->shortcut(), QKeySequence(QStringLiteral("Ctrl+Z")));
        QCOMPARE(redo->shortcut(), QKeySequence(QStringLiteral("Ctrl+Y")));

        QVERIFY2(!undo->isEnabled(), "Undo is enabled on a fresh buffer");
        QVERIFY2(!redo->isEnabled(), "Redo is enabled on a fresh buffer");

        // Undo sits above Cut, as in Kate and Turbo Vision. QAction::menu()
        // returns an action's SUBMENU, not its container, so reach the Edit
        // menu through the menu bar's own actions.
        QMenu* editMenu = nullptr;
        for (QAction* top : mb->actions())
            if (top->menu() && top->text().remove(QLatin1Char('&')).startsWith(
                                   QStringLiteral("Edit")))
                editMenu = top->menu();
        QVERIFY2(editMenu, "Edit menu not found in the menu bar");
        const QList<QAction*> items = editMenu->actions();
        const int undoAt = items.indexOf(undo);
        const int cutAt = items.indexOf(mb->findChild<QAction*>(QStringLiteral("actionEditCut")));
        QVERIFY2(undoAt >= 0 && cutAt >= 0,
                 qPrintable(QStringLiteral("undoAt=%1 cutAt=%2").arg(undoAt).arg(cutAt)));
        QVERIFY2(undoAt < cutAt,
                 qPrintable(QStringLiteral("Undo (index %1) does not precede Cut (index %2)")
                                .arg(undoAt).arg(cutAt)));
    }

    // 2026-11-19: Title Case sits directly under lowercase.
    void edit_menu_offers_title_case_below_lowercase() {
        ShellWindow w;
        QMenuBar* mb = w.menuBar();
        QVERIFY(mb);
        QAction* lower = mb->findChild<QAction*>(QStringLiteral("actionEditLower"));
        QAction* title = mb->findChild<QAction*>(QStringLiteral("actionEditTitle"));
        QVERIFY(lower);
        QVERIFY2(title, "actionEditTitle missing");
        QVERIFY2(title->text().contains(QStringLiteral("Title Case")),
                 qPrintable(title->text()));

        QMenu* editMenu = nullptr;
        for (QAction* top : mb->actions())
            if (top->menu() && top->text().remove(QLatin1Char('&')).startsWith(
                                   QStringLiteral("Edit")))
                editMenu = top->menu();
        QVERIFY(editMenu);
        const QList<QAction*> items = editMenu->actions();
        QCOMPARE(items.indexOf(title), items.indexOf(lower) + 1);
    }

    // 2026-11-20: Select All / Deselect All close the clipboard block, right
    // after Paste. Neither carries a shortcut: Kate's Ctrl+A is not an action
    // in this build, so the menu must not advertise one.
    void edit_menu_offers_select_all_and_deselect_all_after_paste() {
        ShellWindow w;
        QMenuBar* mb = w.menuBar();
        QVERIFY(mb);
        QAction* paste = mb->findChild<QAction*>(QStringLiteral("actionEditPaste"));
        QAction* all = mb->findChild<QAction*>(QStringLiteral("actionEditSelectAll"));
        QAction* none = mb->findChild<QAction*>(QStringLiteral("actionEditDeselectAll"));
        QVERIFY(paste);
        QVERIFY2(all, "actionEditSelectAll missing");
        QVERIFY2(none, "actionEditDeselectAll missing");
        QCOMPARE(all->text().remove(QLatin1Char('&')), QStringLiteral("Select All"));
        QCOMPARE(none->text().remove(QLatin1Char('&')), QStringLiteral("Deselect All"));
        QVERIFY2(!all->shortcut().toString().size(), "Select All must not bind a shortcut");
        QVERIFY2(!none->shortcut().toString().size(), "Deselect All must not bind a shortcut");

        QMenu* editMenu = nullptr;
        for (QAction* top : mb->actions())
            if (top->menu() && top->text().remove(QLatin1Char('&')).startsWith(
                                   QStringLiteral("Edit")))
                editMenu = top->menu();
        QVERIFY(editMenu);
        const QList<QAction*> items = editMenu->actions();
        QCOMPARE(items.indexOf(all), items.indexOf(paste) + 1);
        QCOMPARE(items.indexOf(none), items.indexOf(paste) + 2);
    }

    // 2026-11-19: the line-tools group. Sort Lines heads it; the three new
    // items follow with no submenu, so the Edit menu stays flat like TV.
    void edit_menu_has_a_line_tools_group() {
        ShellWindow w;
        QMenuBar* mb = w.menuBar();
        QVERIFY(mb);
        QMenu* editMenu = nullptr;
        for (QAction* top : mb->actions())
            if (top->menu() && top->text().remove(QLatin1Char('&')).startsWith(
                                   QStringLiteral("Edit")))
                editMenu = top->menu();
        QVERIFY(editMenu);
        const QList<QAction*> items = editMenu->actions();
        auto at = [&items](const char* name) {
            for (int i = 0; i < items.size(); ++i)
                if (items.at(i)->objectName() == QLatin1String(name))
                    return i;
            return -1;
        };
        const int sort = at("actionEditSort");
        QVERIFY2(sort > 0, "actionEditSort missing");
        QCOMPARE(at("actionEditAddEol"), sort + 1);
        QCOMPARE(at("actionEditAddBol"), sort + 2);
        QCOMPARE(at("actionEditRemoveBreaks"), sort + 3);
        QVERIFY2(items.at(sort - 1)->isSeparator(),
                 "the line-tools group must start after a separator");
        for (int i = sort; i <= sort + 3; ++i) {
            QVERIFY2(!items.at(i)->menu(), "line tools must not nest in a submenu");
            QVERIFY2(items.at(i)->isEnabled(), qPrintable(items.at(i)->text()));
        }
        QVERIFY(items.at(at("actionEditAddEol"))
                    ->text().endsWith(QStringLiteral("...")));
        // 2026-11-19 (second round): Add -> Modify, object names unchanged.
        QCOMPARE(items.at(at("actionEditAddEol"))->text(),
                 QStringLiteral("Modify &End of Line..."));
        QCOMPARE(items.at(at("actionEditAddBol"))->text(),
                 QStringLiteral("Modify &Start of Line..."));
    }

    // 2026-11-19: Word Wrap sits at the end of the View menu, checkable, and
    // ON by default (the view default applied in applyTvStyle).
    void view_menu_offers_a_checked_word_wrap_toggle() {
        ShellWindow w;
        QMenuBar* mb = w.menuBar();
        QVERIFY(mb);
        QAction* wrap = mb->findChild<QAction*>(QStringLiteral("actionViewWordWrap"));
        QVERIFY2(wrap, "actionViewWordWrap missing");
        QVERIFY(wrap->isCheckable());
        QVERIFY2(wrap->isChecked(), "word wrap must default to on");
        QVERIFY(wrap->text().contains(QStringLiteral("Word Wrap")));
        QVERIFY2(!wrap->menu(), "Word Wrap must be a toggle, not a submenu");

        QMenu* viewMenu = nullptr;
        for (QAction* top : mb->actions())
            if (top->menu() && top->text().remove(QLatin1Char('&')).startsWith(
                                   QStringLiteral("View")))
                viewMenu = top->menu();
        QVERIFY(viewMenu);
        // Rows: Zoom x3, sep, Minimap, FoldBar, Word Wrap, Nonprintable,
        // Brackets, Go. Go is always last, so wrap sits fourth from the end.
        QCOMPARE(viewMenu->actions().at(viewMenu->actions().size() - 4), wrap);
    }

    // 2026-11-20: Show / Hide Nonprintable Characters is a checkable toggle
    // sitting between Word Wrap and Go. Off by default (the Kate actions start
    // unchecked), and it is NOT a submenu.
    void view_menu_offers_a_nonprintable_toggle_above_go() {
        ShellWindow w;
        QMenuBar* mb = w.menuBar();
        QVERIFY(mb);
        QAction* np = mb->findChild<QAction*>(QStringLiteral("actionViewNonprintable"));
        QVERIFY2(np, "actionViewNonprintable missing");
        QVERIFY(np->isCheckable());
        QVERIFY2(!np->isChecked(), "nonprintable markers must default to off");
        QVERIFY2(!np->menu(), "Nonprintable must be a toggle, not a submenu");
        QVERIFY2(np->text().contains(QStringLiteral("Nonprintable")),
                 qPrintable(np->text()));

        QMenu* viewMenu = nullptr;
        for (QAction* top : mb->actions())
            if (top->menu() && top->text().remove(QLatin1Char('&')).startsWith(
                                   QStringLiteral("View")))
                viewMenu = top->menu();
        QVERIFY(viewMenu);
        QAction* wrap = mb->findChild<QAction*>(QStringLiteral("actionViewWordWrap"));
        QAction* go = mb->findChild<QAction*>(QStringLiteral("actionViewGo"));
        QAction* brackets = mb->findChild<QAction*>(QStringLiteral("actionViewBrackets"));
        QVERIFY(wrap && go);
        QVERIFY(brackets);
        const QList<QAction*> items = viewMenu->actions();
        QCOMPARE(items.indexOf(np), items.indexOf(wrap) + 1);
        QCOMPARE(items.indexOf(brackets), items.indexOf(np) + 1);
        QCOMPARE(items.indexOf(go), items.indexOf(brackets) + 1);
    }

    // 2026-11-19 (second round): View > Go is a plain jump, last in the View menu.
    void view_menu_offers_go_as_its_last_row() {
        ShellWindow w;
        QMenuBar* mb = w.menuBar();
        QVERIFY(mb);
        QAction* go = mb->findChild<QAction*>(QStringLiteral("actionViewGo"));
        QVERIFY2(go, "actionViewGo missing");
        QCOMPARE(go->text().remove(QLatin1Char('&')), QStringLiteral("Go..."));
        QVERIFY(!go->isCheckable());
        QVERIFY2(!go->menu(), "Go must be a plain action, not a submenu");

        QMenu* view = nullptr;
        for (QAction* top : mb->actions())
            if (top->menu() && top->text().remove(QLatin1Char('&')).startsWith(
                                   QStringLiteral("View")))
                view = top->menu();
        QVERIFY(view);
        QCOMPARE(view->actions().last(), go);
    }

    // Task 10: menu entries the composition root wires to real features exist
    // under stable objectNames, and toggles are checkable.
    // User request 2026-11-24: "a recent files display on the file dropdown
    // with the last 5 opened files, toggleable on and off from settings."
    void the_file_menu_lists_the_five_most_recent_files()
    {
        MenuBar bar;
        const QStringList paths = {
            QStringLiteral("/tmp/a.txt"), QStringLiteral("/tmp/b.txt"),
            QStringLiteral("/tmp/c.txt"), QStringLiteral("/tmp/d.txt"),
            QStringLiteral("/tmp/e.txt"), QStringLiteral("/tmp/f.txt"),
            QStringLiteral("/tmp/g.txt"),
        };
        bar.setRecentFiles(paths);

        QAction* entry = bar.findChild<QAction*>(QStringLiteral("actionFileRecentFiles"));
        QVERIFY2(entry, "the File menu has no Recent Files entry");
        QVERIFY2(entry->isVisible(), "the Recent Files entry is hidden with 7 files");
        QMenu* sub = entry->menu();
        QVERIFY2(sub, "the Recent Files entry has no submenu");

        // Last five, newest first: the sixth and seventh are not offered.
        const QList<QAction*> actions = sub->actions();
        QCOMPARE(actions.size(), 5);
        for (int i = 0; i < 5; ++i) {
            QVERIFY2(actions.at(i)->text().contains(paths.at(i)),
                     qPrintable(QStringLiteral("row %1 says '%2', expected %3")
                                    .arg(i).arg(actions.at(i)->text()).arg(paths.at(i))));
            QCOMPARE(actions.at(i)->objectName(),
                     QStringLiteral("actionFileRecent%1").arg(i));
        }
        QCOMPARE(bar.recentFiles(), paths.mid(0, 5));

        // Picking one asks the app to open exactly that file.
        QSignalSpy spy(&bar, &MenuBar::recentFileRequested);
        actions.at(2)->trigger();
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toString(), paths.at(2));

        // An empty store hides the entry: an empty submenu is a dead end.
        bar.setRecentFiles({});
        QVERIFY2(!entry->isVisible(), "Recent Files stays visible with no files");
    }

    // Round 15 (Xadium): "clicking a recent file doesn't open it".
    //
    // BorlandMenu only takes its OWN row path when the popup is deliberately
    // height-capped and the content overflows (scrolls()); StatusSelectors caps
    // every menu that way. That path, activateRow(), used to emit only the
    // MENU's triggered(QAction*) and never called a->trigger(), so the action's
    // own triggered - which MenuBar connects to emit recentFileRequested() -
    // never fired. The menu highlighted the row, closed, and nothing opened.
    //
    // A plain (uncapped) submenu went through QMenu::mousePressEvent instead and
    // worked, which is why the old test (actions.at(2)->trigger(), a direct
    // action call) passed while the real click failed.
    void clicking_a_recent_file_in_a_scrolling_submenu_opens_it()
    {
        QTemporaryDir td;
        QVERIFY(td.isValid());
        QStringList paths;
        for (int i = 0; i < 5; ++i) {
            const QString p = QDir(td.path()).absoluteFilePath(
                QStringLiteral("recent_%1.txt").arg(i));
            QFile f(p);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("x");
            paths << p;
        }

        MenuBar bar;
        bar.resize(800, 600);
        bar.setRecentFiles(paths);

        QAction* entry = bar.findChild<QAction*>(QStringLiteral("actionFileRecentFiles"));
        QVERIFY2(entry, "the File menu has no Recent Files entry");
        auto* sub = static_cast<BorlandMenu*>(entry->menu());
        QVERIFY2(sub, "the Recent Files entry has no BorlandMenu submenu");

        // Cap far below what five rows need so the content genuinely overflows
        // and BorlandMenu::scrolls() - the activateRow() path - is taken.
        sub->setMaximumHeight(90);
        QVERIFY2(sub->scrolls(), "the submenu did not take the scrolling path");

        QSignalSpy spy(&bar, &MenuBar::recentFileRequested);
        sub->popup(bar.mapToGlobal(QPoint(0, bar.height())));
        QCoreApplication::processEvents();
        QVERIFY2(sub->isVisible(), "the recents submenu did not open");

        // Press + release a row through the menu, exactly as ContentBridge
        // delivers a real click.
        const int row = 2;
        const QRect r = sub->actionGeometry(sub->actions().value(row));
        QVERIFY2(!r.isEmpty(), "the row has no geometry");
        const QPoint local = r.center();
        QMouseEvent press(QEvent::MouseButtonPress, local, sub->mapToGlobal(local),
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(sub, &press);
        QMouseEvent release(QEvent::MouseButtonRelease, local, sub->mapToGlobal(local),
                            Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(sub, &release);
        QCoreApplication::processEvents();

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toString(), paths.at(row));
    }

    // Round 16 (Xadium): "the recent files still don't launch if clicked" - the
    // PRODUCTION path, with BOTH the File menu and the recents submenu open and
    // registered. Round 15 proved activateRow() fires the action, but in the real
    // app the parent File dropdown and the submenu overlap the same point, and
    // ContentBridge::popupAt() returned the FIRST registered popup containing it
    // - the File menu - so the press dismissed the File menu and never reached
    // the recents row. This test opens both menus, registers both (exactly as
    // main.cpp's PopupRegistrar does on Show), then clicks a row through the
    // bridge the way QML delivers it.
    void clicking_a_recent_file_with_the_file_menu_open_launches_it()
    {
        QTemporaryDir td;
        QVERIFY(td.isValid());
        QStringList paths;
        for (int i = 0; i < 3; ++i) {
            const QString p = QDir(td.path()).absoluteFilePath(
                QStringLiteral("live_recent_%1.txt").arg(i));
            QFile f(p);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("x");
            paths << p;
        }

        ShellWindow shell;
        shell.resize(1000, 700);
        ContentBridge bridge;
        bridge.setShellWindow(&shell);
        shell.show();
        QCoreApplication::processEvents();

        MenuBar* bar = shell.findChild<MenuBar*>();
        QVERIFY2(bar, "the shell has no MenuBar");
        bar->setRecentFiles(paths);

        QAction* entry = bar->findChild<QAction*>(QStringLiteral("actionFileRecentFiles"));
        QVERIFY2(entry, "the File menu has no Recent Files entry");
        QMenu* sub = entry->menu();
        QVERIFY2(sub, "the Recent Files entry has no submenu");

        // Open the File dropdown, then the recents submenu - both stay visible
        // and both register with the bridge, exactly as the live app does.
        QMenu* fileMenu = nullptr;
        for (QAction* a : bar->actions())
            if (a->menu() && a->text().remove(QLatin1Char('&')) == QLatin1String("File"))
                fileMenu = a->menu();
        QVERIFY2(fileMenu, "no File menu");
        fileMenu->popup(bar->mapToGlobal(QPoint(0, bar->height())));
        QCoreApplication::processEvents();
        QVERIFY2(fileMenu->isVisible(), "the File menu did not open");

        // The recents submenu opens from the Recent Files entry INSIDE the File
        // menu, so it overlaps the File dropdown - BOTH registered popups then
        // contain the row's point, the exact collision the bridge mis-routes.
        QVERIFY2(fileMenu->isVisible(), "the File menu did not open");
        // Open the recents submenu from the Recent Files entry, then (matching a
        // live File menu where the entry sits low) keep it overlapping the File
        // dropdown's lower band. popup() places relative to the parent menu; if
        // the natural spot leaves the row outside the File menu, re-popup higher
        // so BOTH registered popups contain the row - the collision popupAt()
        // breaks on.
        const QRect entryGeom = fileMenu->actionGeometry(entry);
        QVERIFY2(!entryGeom.isEmpty(), "the Recent Files entry has no geometry");
        QAction* row = sub->actions().value(1);
        QVERIFY2(row, "the recents submenu has no rows");

        // Open once to measure where row 1 actually lands, then re-open the
        // submenu so row 1's centre sits just inside the File menu's lower band
        // - the live layout, where the recents entry is near the bottom of a
        // full File menu and its submenu overlaps the dropdown.
        sub->popup(fileMenu->mapToGlobal(QPoint(entryGeom.left() + 4, entryGeom.top())));
        QCoreApplication::processEvents();
        QVERIFY2(sub->isVisible(), "the recents submenu did not open");
        const int rowOffY = sub->mapToGlobal(sub->actionGeometry(row).center()).y() - sub->y();
        const int targetY = fileMenu->mapToGlobal(QPoint(0, fileMenu->height() - 10)).y();
        sub->popup(QPoint(sub->x(), targetY - rowOffY));
        QCoreApplication::processEvents();
        QVERIFY2(sub->isVisible(), "the recents submenu closed on reposition");
        bridge.registerPopup(fileMenu);
        bridge.registerPopup(sub);

        // Row centre in GLOBAL, then SHELL-local - what QML hands the bridge.
        const QPoint rowGlobal = sub->mapToGlobal(sub->actionGeometry(row).center());
        const QPoint shellLocal = shell.mapFromGlobal(rowGlobal);
        QVERIFY2(fileMenu->rect().contains(fileMenu->mapFromGlobal(rowGlobal)),
                 qPrintable(QStringLiteral("row not in File menu (rowGlobal=%1,%2 fileMenu=%3,%4 %5x%6)")
                                .arg(rowGlobal.x()).arg(rowGlobal.y())
                                .arg(fileMenu->x()).arg(fileMenu->y())
                                .arg(fileMenu->width()).arg(fileMenu->height())));
        QVERIFY2(sub->rect().contains(sub->mapFromGlobal(rowGlobal)),
                 "the row is not in the recents submenu either");

        QSignalSpy spy(bar, &MenuBar::recentFileRequested);
        bridge.forwardMouseEvent(QEvent::MouseButtonPress, Qt::LeftButton,

                                 Qt::LeftButton, Qt::NoModifier,
                                 shellLocal.x(), shellLocal.y());
        bridge.forwardMouseEvent(QEvent::MouseButtonRelease, Qt::LeftButton,
                                 Qt::NoButton, Qt::NoModifier,
                                 shellLocal.x(), shellLocal.y());
        QCoreApplication::processEvents();

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toString(), paths.at(1));
    }

    // Round 17 (Xadium): "The links from recent files still don't navigate
    // anywhere."
    //
    // Round 16 fixed WHICH popup receives the press (the innermost submenu).
    // The DISMISS loop above it was still wrong: on a press it walks the
    // registered popups and sends a dismissing press into the FIRST visible one
    // that does not contain the point. In the live layout the recents submenu
    // hangs to the RIGHT of (and usually below) the File dropdown, so the File
    // menu does not contain the row - it receives the dismiss press, hides, and
    // takes its submenu down with it. The release then lands on a hidden menu
    // and no action fires. A popup that OWNS the click's target must never be
    // dismissed.
    void clicking_a_recent_file_hanging_outside_the_file_menu_opens_it()
    {
        QTemporaryDir td;
        QVERIFY(td.isValid());
        QStringList paths;
        for (int i = 0; i < 3; ++i) {
            const QString p = QDir(td.path()).absoluteFilePath(
                QStringLiteral("live_recent_%1.txt").arg(i));
            QFile f(p);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("x");
            paths << p;
        }

        ShellWindow shell;
        shell.resize(1000, 700);
        ContentBridge bridge;
        bridge.setShellWindow(&shell);
        shell.show();
        QCoreApplication::processEvents();

        MenuBar* bar = shell.findChild<MenuBar*>();
        QVERIFY2(bar, "the shell has no MenuBar");
        bar->setRecentFiles(paths);

        QAction* entry = bar->findChild<QAction*>(QStringLiteral("actionFileRecentFiles"));
        QVERIFY2(entry, "the File menu has no Recent Files entry");
        QMenu* sub = entry->menu();
        QVERIFY2(sub, "the Recent Files entry has no submenu");

        QMenu* fileMenu = nullptr;
        for (QAction* a : bar->actions())
            if (a->menu() && a->text().remove(QLatin1Char('&')) == QLatin1String("File"))
                fileMenu = a->menu();
        QVERIFY2(fileMenu, "no File menu");
        fileMenu->popup(bar->mapToGlobal(QPoint(0, bar->height())));
        QCoreApplication::processEvents();
        QVERIFY2(fileMenu->isVisible(), "the File menu did not open");

        // Register in the live order: the parent dropdown first, the submenu
        // second (PopupRegistrar registers on Show, parent shows first).
        bridge.registerPopup(fileMenu);

        QAction* row = sub->actions().value(1);
        QVERIFY2(row, "the recents submenu has no rows");
        const QRect entryGeom = fileMenu->actionGeometry(entry);
        QVERIFY2(!entryGeom.isEmpty(), "the Recent Files entry has no geometry");
        // The live geometry: the submenu hangs off the RIGHT edge of the File
        // dropdown, so the row is OUTSIDE the parent menu's rect.
        sub->popup(fileMenu->mapToGlobal(
            QPoint(fileMenu->width() + 8, entryGeom.top())));
        QCoreApplication::processEvents();
        QVERIFY2(sub->isVisible(), "the recents submenu did not open");
        bridge.registerPopup(sub);

        const QPoint rowGlobal = sub->mapToGlobal(sub->actionGeometry(row).center());
        const QPoint shellLocal = shell.mapFromGlobal(rowGlobal);
        // Preconditions: the row is in the SUBMENU and NOT in the File menu.
        QVERIFY2(sub->rect().contains(sub->mapFromGlobal(rowGlobal)),
                 "the row is not in the recents submenu");
        QVERIFY2(!fileMenu->rect().contains(fileMenu->mapFromGlobal(rowGlobal)),
                 "test setup is wrong: the row overlaps the File menu");

        QSignalSpy spy(bar, &MenuBar::recentFileRequested);
        // A real pointer always hovers the row before pressing; QMenu only
        // selects the item once the move has registered it, so the hover first,
        // then the press/release pair that must activate it.
        bridge.forwardMouseEvent(QEvent::MouseMove, Qt::NoButton, Qt::NoButton,
                                 Qt::NoModifier, shellLocal.x(), shellLocal.y());
        QCoreApplication::processEvents();
        bridge.forwardMouseEvent(QEvent::MouseButtonPress, Qt::LeftButton,
                                 Qt::LeftButton, Qt::NoModifier,
                                 shellLocal.x(), shellLocal.y());
        QCoreApplication::processEvents();
        QVERIFY2(sub->isVisible(),
                 "the press dismissed the submenu before the release");
        bridge.forwardMouseEvent(QEvent::MouseButtonRelease, Qt::LeftButton,
                                 Qt::NoButton, Qt::NoModifier,
                                 shellLocal.x(), shellLocal.y());
        QCoreApplication::processEvents();

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toString(), paths.at(1));
        // The guard's own RED/GREEN check (mutation 2026-11-26): without the
        // ownsTarget skip in ContentBridge's dismiss loop, this press also
        // went to the File menu, which hid and took the submenu down with it -
        // spy stayed 0. Also proven on the LIVE app's own log: every pre-fix
        // recents click read press hit=menuFileRecentFiles then release
        // hit=KateViewInternal (the menu was gone by release time).
        QVERIFY2(sub->isVisible() || spy.count() == 1,
                 "the submenu was dismissed by the press before the release");
    }

    // Round 17 (Xadium): "The links from recent files still don't navigate
    // anywhere." Round 16 proved recentFileRequested FIRES; this proves the
    // WHOLE chain the user actually cares about - the click opens a TAB. It
    // wires recentFileRequested -> EditorTabManager::openFile exactly as the
    // composition root does (main.cpp), then clicks a recent row through the
    // bridge and asserts a document tab appears with that file's URL. This is
    // the coverage round 16 was missing (it only counted the signal).
    void clicking_a_recent_file_opens_a_document_tab()
    {
        QTemporaryDir td;
        QVERIFY(td.isValid());
        QStringList paths;
        for (int i = 0; i < 3; ++i) {
            const QString p = QDir(td.path()).absoluteFilePath(
                QStringLiteral("navigating_recent_%1.txt").arg(i));
            QFile f(p);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(QStringLiteral("content %1").arg(i).toUtf8());
            paths << p;
        }

        ShellWindow shell;
        shell.resize(1000, 700);
        ContentBridge bridge;
        bridge.setShellWindow(&shell);
        shell.show();
        QCoreApplication::processEvents();

        MenuBar* bar = shell.findChild<MenuBar*>();
        QVERIFY2(bar, "the shell has no MenuBar");

        // The tab manager the composition root would own, plus the exact
        // recentFileRequested -> openFile wiring from main.cpp:754.
        EditorTabManager tabs;
        const int before = tabs.viewCount();
        QObject::connect(bar, &MenuBar::recentFileRequested, bar,
                         [&tabs](const QString& path) {
                             if (path.isEmpty() || !QFileInfo::exists(path))
                                 return;
                             tabs.openFile(QUrl::fromLocalFile(path));
                         });

        bar->setRecentFiles(paths);
        QAction* entry = bar->findChild<QAction*>(QStringLiteral("actionFileRecentFiles"));
        QVERIFY2(entry, "the File menu has no Recent Files entry");
        QMenu* sub = entry->menu();
        QVERIFY2(sub, "the Recent Files entry has no submenu");

        QMenu* fileMenu = nullptr;
        for (QAction* a : bar->actions())
            if (a->menu() && a->text().remove(QLatin1Char('&')) == QLatin1String("File"))
                fileMenu = a->menu();
        QVERIFY2(fileMenu, "no File menu");
        fileMenu->popup(bar->mapToGlobal(QPoint(0, bar->height())));
        QCoreApplication::processEvents();
        QVERIFY2(fileMenu->isVisible(), "the File menu did not open");
        bridge.registerPopup(fileMenu);

        QAction* row = sub->actions().value(1);
        QVERIFY2(row, "the recents submenu has no rows");
        sub->popup(fileMenu->mapToGlobal(QPoint(20, fileMenu->actionGeometry(entry).top())));
        QCoreApplication::processEvents();
        QVERIFY2(sub->isVisible(), "the recents submenu did not open");
        bridge.registerPopup(sub);

        const QPoint rowGlobal = sub->mapToGlobal(sub->actionGeometry(row).center());
        const QPoint shellLocal = shell.mapFromGlobal(rowGlobal);
        // Hover the row first - a real pointer always does - so QMenu selects
        // it before the press.
        bridge.forwardMouseEvent(QEvent::MouseMove, Qt::NoButton, Qt::NoButton,
                                 Qt::NoModifier, shellLocal.x(), shellLocal.y());
        QCoreApplication::processEvents();
        bridge.forwardMouseEvent(QEvent::MouseButtonPress, Qt::LeftButton,
                                 Qt::LeftButton, Qt::NoModifier,
                                 shellLocal.x(), shellLocal.y());
        bridge.forwardMouseEvent(QEvent::MouseButtonRelease, Qt::LeftButton,
                                 Qt::NoButton, Qt::NoModifier,
                                 shellLocal.x(), shellLocal.y());
        QCoreApplication::processEvents();

        // THE NAVIGATION: a document tab opened and it is the clicked file.
        QCOMPARE(tabs.viewCount(), before + 1);
        QVERIFY(tabs.activeDocument());
        QCOMPARE(tabs.activeDocument()->url(), QUrl::fromLocalFile(paths.at(1)));
    }

    // Settings switch: the whole Recent Files block can be turned off.
    void the_recent_files_entry_can_be_hidden_from_settings()
    {
        MenuBar bar;
        bar.setRecentFiles({ QStringLiteral("/tmp/a.txt") });
        QAction* entry = bar.findChild<QAction*>(QStringLiteral("actionFileRecentFiles"));
        QVERIFY(entry);

        bar.setRecentFilesVisible(false);
        QVERIFY2(!entry->isVisible(), "the setting did not hide Recent Files");

        bar.setRecentFilesVisible(true);
        QVERIFY2(entry->isVisible(), "turning the setting back on did not restore it");

        // ...and a store update must not resurrect it while it is off.
        bar.setRecentFilesVisible(false);
        bar.setRecentFiles({ QStringLiteral("/tmp/b.txt") });
        QVERIFY2(!entry->isVisible(), "a new recent file ignored the off setting");
    }

    void menu_entries_expose_wiring_targets() {
        ShellWindow w;
        QMenuBar* mb = w.menuBar();
        QVERIFY(mb);

        const QStringList required = {
            QStringLiteral("actionFileNew"),   QStringLiteral("actionFileOpen"),
            // Split view (2026-11-21): File > Open as Split Document...
            QStringLiteral("actionFileOpenAsSplit"),
            QStringLiteral("actionFileSave"),  QStringLiteral("actionFileSaveAs"),
            QStringLiteral("actionEditUndo"),  QStringLiteral("actionEditRedo"),
            QStringLiteral("actionEditCut"),   QStringLiteral("actionEditCopy"),
            QStringLiteral("actionEditPaste"),
            QStringLiteral("actionEditSelectAll"), QStringLiteral("actionEditDeselectAll"),
            QStringLiteral("actionEditUpper"), QStringLiteral("actionEditLower"),
            QStringLiteral("actionEditTitle"),
            QStringLiteral("actionEditSort"),  QStringLiteral("actionEditSpellcheck"),
            QStringLiteral("actionEditAddEol"), QStringLiteral("actionEditAddBol"),
            QStringLiteral("actionEditRemoveBreaks"),
            QStringLiteral("actionSearchFind"),
            QStringLiteral("actionSearchFindNext"),
            // Find and Replace (2026-11-18): the Search menu offers Replace.
            QStringLiteral("actionSearchReplace"),
            QStringLiteral("actionViewZoomIn"), QStringLiteral("actionViewZoomOut"),
            QStringLiteral("actionViewZoomReset"),
            QStringLiteral("actionViewMinimap"), QStringLiteral("actionViewFoldBar"),
            QStringLiteral("actionViewCollapseAll"),
            QStringLiteral("actionViewExpandAll"),
            QStringLiteral("actionViewToggleNode"),
            QStringLiteral("actionViewWordWrap"),
            QStringLiteral("actionViewNonprintable"),
            QStringLiteral("actionViewBrackets"),
            QStringLiteral("actionViewGo"),
            // Split view (2026-11-21): View > Open Document in New View.
            QStringLiteral("actionViewOpenInNewView"),
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
                                     QStringLiteral("actionViewNonprintable"),
                                     QStringLiteral("actionViewBrackets"),
                                     QStringLiteral("actionEditSpellcheck") }) {
            QAction* a = mb->findChild<QAction*>(name);
            QVERIFY2(a && a->isCheckable(),
                     qPrintable(QStringLiteral("'%1' should be a checkable toggle").arg(name)));
        }
        // Split actions are one-shot commands, NOT checkable toggles.
        for (const QString& name : { QStringLiteral("actionFileOpenAsSplit"),
                                     QStringLiteral("actionViewOpenInNewView") }) {
            QAction* a = mb->findChild<QAction*>(name);
            QVERIFY2(a && !a->isCheckable(),
                     qPrintable(QStringLiteral("'%1' should be a non-checkable action").arg(name)));
        }
    }

    // Task 12 (Turbo Vision spec): menu bar = light-gray #AAAAAA bg, BLACK
    // label text; window blue = #0000AA; hotkey = red #AA0000 (a QAction's
    // highlighted role doubles as the hotkey letter in this theme).
    // Task 3: canonical Turbo Vision placement — the first top-level title
    // starts at ~column 2 (2 cell widths from the left) and consecutive titles
    // are separated by a small, consistent gap (TV's fixed-column layout), not
    // QMenuBar's default flush packing.
    // TV-revision spec 2: the menu bar is ONE flat #AAAAAA row (row 0),
    // full width, no texture — today the bar is ~84px (a 3-row slab).
    void menu_bar_is_one_flat_row() {
        ShellWindow w;
        w.resize(800, 600);
        w.show();
        QMenuBar* mb = w.menuBar();
        QVERIFY(mb);
        const QList<QAction*> tops = mb->actions();
        QVERIFY(!tops.isEmpty());
        const QRect g0 = mb->actionGeometry(tops.first());
        {
            const QFontMetrics fmProbe(mb->font());
            QString geom;
            for (QAction* a : tops) {
                const QRect g = mb->actionGeometry(a);
                geom += QStringLiteral("%1@(x%2,y%3,w%4,h%5) ")
                            .arg(a->text().remove(QLatin1Char('&')))
                            .arg(g.x()).arg(g.y()).arg(g.width()).arg(g.height());
            }
            const QSize minHint = mb->minimumSizeHint();
            QVERIFY2(mb->height() <= 28,
                     qPrintable(QStringLiteral(
                         "menubar height=%1 sizeHint=%2x%3 minHint=%4x%5 "
                         "style=%6 font='%7' px=%8 fm.h=%9 adv(s)=%10 adv(M)=%11 | %12")
                                    .arg(mb->height())
                                    .arg(mb->sizeHint().width()).arg(mb->sizeHint().height())
                                    .arg(minHint.width()).arg(minHint.height())
                                    .arg(QLatin1String(mb->style()->metaObject()->className()))
                                    .arg(mb->font().family())
                                    .arg(mb->font().pixelSize())
                                    .arg(fmProbe.height())
                                    .arg(fmProbe.horizontalAdvance(QLatin1Char(' ')))
                                    .arg(fmProbe.horizontalAdvance(QLatin1Char('M')))
                                    .arg(geom)));
        }

        // Flat gray: sample rows near the top/bottom edges (never text rows)
        // across the full width, plus the side margins on the middle row.
        QImage img = mb->grab().toImage().convertToFormat(QImage::Format_RGB32);
        int nonGray = 0;
        QString bad;
        const int xs[] = { 1, img.width() / 5, 2 * img.width() / 5, 3 * img.width() / 5,
                           4 * img.width() / 5, img.width() - 3 };
        struct Sample { int x, y; };
        QList<Sample> samples;
        for (int x : xs)
            samples << Sample{ x, 1 };
        for (int x : xs)
            samples << Sample{ x, img.height() - 2 };
        for (int x : { 1, img.width() - 3 })
            samples << Sample{ x, img.height() / 2 };
        for (const Sample& s : samples) {
            const QColor c = img.pixelColor(s.x, s.y);
            if (qAbs(c.red() - 0xAA) > 12 || qAbs(c.green() - 0xAA) > 12
                || qAbs(c.blue() - 0xAA) > 12) {
                ++nonGray;
                bad += QStringLiteral("(%1,%2)=#%3%4%5 ").arg(s.x).arg(s.y)
                           .arg(c.red(), 2, 16, QLatin1Char('0'))
                           .arg(c.green(), 2, 16, QLatin1Char('0'))
                           .arg(c.blue(), 2, 16, QLatin1Char('0'));
            }
        }
        QVERIFY2(nonGray == 0,
                 qPrintable(QStringLiteral("menu bar not flat #AAAAAA at %1 samples: %2")
                                .arg(nonGray).arg(bad)));
    }

    // TV-revision spec 2: FIRST letter of every title (F, E, S, V, S, S, H)
    // is hotkey red #AA0000; the rest of the label stays black.
    // TV: the FIRST visible letter of each menu-bar title is hotkey-red.
    // BorlandStyle CENTERS the title in its action rect (so the selected green
    // plate is balanced), so the first glyph is NOT at rect.x() - locate the
    // leftmost actual glyph inside the item and check its colour. Sampling a
    // fixed box at rect.x() reads background whenever centering shifts the text.
    void menu_first_letter_of_each_item_is_red() {
        ShellWindow w;
        w.resize(900, 600);
        w.show();
        QMenuBar* mb = w.menuBar();
        QVERIFY(mb);
        QImage img = mb->grab().toImage().convertToFormat(QImage::Format_RGB32);
        const auto isBg = [](const QColor& c) {
            return qAbs(c.red() - 0xAA) <= 45 && qAbs(c.green() - 0xAA) <= 45
                && qAbs(c.blue() - 0xAA) <= 45;   // menu-bar gray
        };

        for (QAction* a : mb->actions()) {
            const QRect g = mb->actionGeometry(a);
            // Leftmost non-background (glyph) pixel column in the item.
            int glyphX = -1;
            for (int x = g.left(); x <= g.right() && glyphX < 0; ++x)
                for (int y = g.top(); y <= g.bottom(); ++y) {
                    if (x < 0 || y < 0 || x >= img.width() || y >= img.height())
                        continue;
                    if (!isBg(img.pixelColor(x, y))) { glyphX = x; break; }
                }
            QVERIFY2(glyphX >= 0,
                     qPrintable(QStringLiteral("menu item '%1': no glyph painted in its rect")
                                    .arg(a->text().remove(QLatin1Char('&')))));
            // Sample that first glyph's cell for hotkey-red.
            int red = 0;
            for (int x = glyphX; x < glyphX + 8 && x <= g.right(); ++x)
                for (int y = g.top(); y <= g.bottom(); ++y) {
                    if (x < 0 || y < 0 || x >= img.width() || y >= img.height())
                        continue;
                    const QColor c = img.pixelColor(x, y);
                    if (c.red() > 150 && c.green() < 90 && c.blue() < 90)
                        ++red;
                }
            QVERIFY2(red >= 1,
                     qPrintable(QStringLiteral(
                         "menu item '%1': first glyph is not hotkey-red (red=%2)")
                                    .arg(a->text().remove(QLatin1Char('&'))).arg(red)));
        }
    }

    // TV-revision spec 2: about FOUR spaces between titles. Measured VISUALLY:
    // distance from the end of one title's text to the start of the next
    // (title start = action geometry x since BorlandStyle paints glyphs
    // flush at rect.x). Old packed gap was 26px (RED); target ~40px.
    void menu_titles_separated_by_four_spaces() {
        ShellWindow w;
        w.resize(900, 600);
        w.show();
        QMenuBar* mb = w.menuBar();
        QVERIFY(mb);
        const QList<QAction*> tops = mb->actions();
        QVERIFY(tops.size() >= 2);
        const QFontMetrics fm(mb->font());
        const int cellW = fm.horizontalAdvance(QLatin1Char('M'));
        for (int i = 1; i < tops.size(); ++i) {
            const QRect prev = mb->actionGeometry(tops.at(i - 1));
            const QRect cur = mb->actionGeometry(tops.at(i));
            const QString prevTitle = tops.at(i - 1)->text().remove(QLatin1Char('&'));
            const int gap = cur.x() - prev.x() - fm.horizontalAdvance(prevTitle);
            QVERIFY2(gap >= 2 * cellW && gap <= 4 * cellW,
                     qPrintable(QStringLiteral(
                         "visual gap '%1'|'%2' = %3 px, expected ~4 spaces (%4..%5 px, cellW=%6)")
                                    .arg(prevTitle)
                                    .arg(tops.at(i)->text().remove(QLatin1Char('&')))
                                    .arg(gap).arg(2 * cellW).arg(4 * cellW).arg(cellW)));
        }
    }

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
        QVERIFY2(hl.red() > 0x9E && hl.red() < 0xC0 &&
                 hl.green() < 0x18 && hl.blue() < 0x18,
                 qPrintable(QStringLiteral("hotkey is not TV red #AA0000: #%1%2%3")
                                .arg(hl.red(), 2, 16, QLatin1Char('0'))
                                .arg(hl.green(), 2, 16, QLatin1Char('0'))
                                .arg(hl.blue(), 2, 16, QLatin1Char('0'))));
    }

    // Task 12: window/desktop blue is TV #0000AA (not old navy #0000aa),
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
                if (qAbs(c.red() - 0x00) <= 8 && qAbs(c.green() - 0x00) <= 8 &&
                    qAbs(c.blue() - 0xAA) <= 10)
                    ++blue;
            }
        QVERIFY2(blue > 1000,
                 qPrintable(QStringLiteral("TV-blue #0000AA px=%1, expected >1000").arg(blue)));
    }

    // ---- TV-revision2 Task 5 (spec 5 + 6) --------------------------------
    // Helper: open the View dropdown through the bridge (same hardening the
    // app uses) so grabs see real menu pixels.
    static QMenu* openViewMenu(ShellWindow& w, ContentBridge& b) {
        auto* mb = static_cast<MenuBar*>(w.menuBar());
        if (!mb)
            return nullptr;
        QMenu* view = mb->menu(QStringLiteral("View"));
        if (!view)
            return nullptr;
        b.registerPopup(view);
        view->show();
        QCoreApplication::processEvents();
        return view;
    }
    static QAction* findAction(QMenu* m, const QString& needle) {
        for (QAction* a : m->actions())
            if (!a->isSeparator() && a->text().contains(needle))
                return a;
        return nullptr;
    }
    static QPoint itemCenterInShell(QMenu* menu, QAction* a, ShellWindow* w) {
        return menu->pos() - w->pos() + menu->actionGeometry(a).center();
    }

    // Reference screenshot: the OPEN menu's title is a GREEN plate with BLACK
    // text; the hotkey letter stays red. (This supersedes rev2 spec 5's
    // black-plate/white-text rule - see HANDOFF P11.)
    void selected_menu_title_is_green_plate_black_text_red_hotkey() {
        ShellWindow w;
        BorlandStyle* st = w.borlandStyle();
        QVERIFY(st);
        QImage img(120, 26, QImage::Format_RGB32);
        img.fill(QColor(0xAA, 0xAA, 0xAA));
        QPainter p(&img);
        p.setFont(w.font());
        QStyleOptionMenuItem opt;
        opt.rect = QRect(0, 0, 120, 26);
        opt.text = QStringLiteral("&View");
        opt.menuItemType = QStyleOptionMenuItem::Normal;
        opt.state = QStyle::State_Selected | QStyle::State_Enabled;
        st->drawControl(QStyle::CE_MenuBarItem, &opt, &p, &w);
        p.end();

        int green = 0, black = 0, red = 0, total = 0;
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x) {
                const QColor c = img.pixelColor(x, y);
                ++total;
                if (c.green() > 0x88 && c.red() < 0x50 && c.blue() < 0x50) ++green;
                if (c.red() < 40 && c.green() < 40 && c.blue() < 40) ++black;
                if (c.red() > 120 && c.red() < 200 && c.green() < 40 && c.blue() < 40) ++red;
            }
        QVERIFY2(green * 2 > total,
                 qPrintable(QStringLiteral("selected title plate not green: %1/%2")
                                .arg(green).arg(total)));
        QVERIFY2(black >= 5,
                 qPrintable(QStringLiteral("selected title text not black: %1 px").arg(black)));
        QVERIFY2(red >= 2,
                 qPrintable(QStringLiteral("selected title hotkey not red: %1 px").arg(red)));

        // Screenshot styling: the hotkey letter is underlined (a short white
        // horizontal run below the glyph baseline).
        int underline = 0;
        for (int y = 0; y < img.height(); ++y) {
            int run = 0, best = 0;
            for (int x = 0; x < img.width(); ++x) {
                const QColor c = img.pixelColor(x, y);
                if (c.red() < 40 && c.green() < 40 && c.blue() < 40)
                    best = qMax(best, ++run);
                else
                    run = 0;
            }
            if (best >= 4)
                underline = qMax(underline, best);
        }
        QVERIFY2(underline >= 4,
                 qPrintable(QStringLiteral("hotkey underline missing (best run %1)")
                                .arg(underline)));
    }

    // Clicking anywhere OUTSIDE an open dropdown must dismiss it. The app
    // synthesizes mouse events (the shell is composited offscreen), so a press
    // that misses the popup used to be routed straight to the editor and the
    // menu stayed open - QMenu's own outside-click handling never saw it.
    void click_outside_dropdown_dismisses_it() {
        ShellWindow w;
        w.resize(900, 600);
        ContentBridge b;
        b.setShellWindow(&w);
        QMenu* view = openViewMenu(w, b);
        QVERIFY2(view, "View menu missing");
        QVERIFY2(view->isVisible(), "menu should be open before the click");

        // A point in the editor body, well clear of the dropdown.
        const QPoint outside(120, 520);
        QVERIFY2(!view->rect().translated(view->pos()).contains(outside),
                 "test point must be outside the popup");

        // Count presses that land anywhere OTHER than the popup (the popup's
        // own dismissal press must not be mistaken for the editor's).
        struct PressCounter : QObject {
            QMenu* popup = nullptr;
            bool reached = false;
            bool eventFilter(QObject* o, QEvent* e) override {
                if (e->type() == QEvent::MouseButtonPress && o != popup)
                    reached = true;
                return false;
            }
        } counter;
        counter.popup = view;
        qApp->installEventFilter(&counter);

        b.forwardMouseEvent(QEvent::MouseButtonPress, Qt::LeftButton, Qt::LeftButton,
                            Qt::NoModifier, outside.x(), outside.y());
        qApp->removeEventFilter(&counter);
        const bool reachedEditor = counter.reached;
        b.forwardMouseEvent(QEvent::MouseButtonRelease, Qt::LeftButton, Qt::NoButton,
                            Qt::NoModifier, outside.x(), outside.y());
        QCoreApplication::processEvents();

        QVERIFY2(!view->isVisible(),
                 "dropdown stayed open after a click outside it");
        // The dismissing click must ALSO reach the widget underneath, so it
        // places the caret instead of being swallowed.
        QVERIFY2(reachedEditor,
                 "dismissing click was swallowed - it must also reach the editor");
    }

    // Reference screenshot: dropdown is a solid light-gray panel whose single
    // border is BLACK and inset (never a white line-drawing border). The gray
    // fill must still dominate the panel.
    void dropdown_is_flat_gray_with_black_border() {
        ShellWindow w;
        w.resize(900, 600);
        ContentBridge b;
        b.setShellWindow(&w);
        QMenu* view = openViewMenu(w, b);
        QVERIFY2(view, "View menu missing");
        const QImage img = view->grab().toImage();
        QVERIFY(!img.isNull());

        int gray = 0, total = 0;
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x) {
                const QColor c = img.pixelColor(x, y);
                ++total;
                if (qAbs(c.red() - 0xAA) < 30 && qAbs(c.green() - 0xAA) < 30
                    && qAbs(c.blue() - 0xAA) < 30)
                    ++gray;
            }
        QVERIFY2(gray * 2 >= total,
                 qPrintable(QStringLiteral("dropdown not flat gray: %1/%2").arg(gray).arg(total)));

        int whiteEdge = 0;
        for (int x = 0; x < img.width(); ++x) {
            const QColor c = img.pixelColor(x, 0);
            if (c.red() > 235 && c.green() > 235 && c.blue() > 235) ++whiteEdge;
        }
        for (int y = 0; y < img.height(); ++y) {
            const QColor c = img.pixelColor(0, y);
            if (c.red() > 235 && c.green() > 235 && c.blue() > 235) ++whiteEdge;
        }
        QVERIFY2(whiteEdge == 0,
                 qPrintable(QStringLiteral("dropdown has a white border line: %1 px")
                                .arg(whiteEdge)));
        b.unregisterPopup(view);
        view->hide();
    }

    // Spec 6: the highlighted row is a FULL-WIDTH green bar (edge to edge),
    // text black, hotkey red. Driven by a forwarded hover.
    void dropdown_selected_row_is_green_full_width() {
        ShellWindow w;
        w.resize(900, 600);
        ContentBridge b;
        b.setShellWindow(&w);
        QMenu* view = openViewMenu(w, b);
        QVERIFY2(view, "View menu missing");
        QAction* zoomIn = findAction(view, QStringLiteral("In"));
        QVERIFY2(zoomIn, "Zoom In missing");
        const QPoint pt = itemCenterInShell(view, zoomIn, &w);
        b.forwardMouseEvent(QEvent::MouseMove, Qt::NoButton, Qt::NoButton,
                            Qt::NoModifier, pt.x(), pt.y());
        QCoreApplication::processEvents();

        const QImage img = view->grab().toImage();
        const QRect r = view->actionGeometry(zoomIn);
        const int y = r.center().y();
        int green = 0, black = 0, red = 0;
        for (int x = 0; x < img.width(); ++x) {
            const QColor c = img.pixelColor(x, y);
            if (c.green() > 140 && c.red() < 60 && c.blue() < 60) ++green;
            if (c.red() < 40 && c.green() < 40 && c.blue() < 40) ++black;
            if (c.red() > 120 && c.red() < 200 && c.green() < 40 && c.blue() < 40) ++red;
        }
        const auto isGreen = [&](const QColor& c) {
            return c.green() > 140 && c.red() < 60 && c.blue() < 60;
        };
        // p13: the green bar spans the full width of the area INSIDE the inset
        // black border (the gray margin outside the border is not highlighted).
        // The green bar is inset from the stroke by highlightInset() on both
        // sides, leaving panel gray either side of it.
        const int edge = BorlandMenu::borderInset() + BorlandMenu::borderWidth()
            + BorlandMenu::highlightInset();
        const int innerLeft = edge, innerRight = img.width() - edge - 1;
        QVERIFY2(isGreen(img.pixelColor(innerLeft, y)) && isGreen(img.pixelColor(innerRight, y)),
                 qPrintable(QStringLiteral("selected row not edge-to-edge green inside the border "
                                           "(edges: %1/%2)")
                                .arg(img.pixelColor(innerLeft, y).name(),
                                     img.pixelColor(innerRight, y).name())));
        // And the margin OUTSIDE the border stays panel gray.
        QVERIFY2(!isGreen(img.pixelColor(0, y)),
                 "highlight must not run outside the inset border");
        // A gray gap separates the green bar from the border stroke on both
        // sides (reference screenshot: ~3 px either side).
        const int strokeEdge = BorlandMenu::borderInset() + BorlandMenu::borderWidth();
        const auto isGrayPx = [](const QColor& c) {
            return qAbs(c.red() - 0xAA) < 20 && qAbs(c.green() - 0xAA) < 20
                && qAbs(c.blue() - 0xAA) < 20;
        };
        QVERIFY2(isGrayPx(img.pixelColor(strokeEdge, y))
                     && isGrayPx(img.pixelColor(img.width() - strokeEdge - 1, y)),
                 "expected gray between the border stroke and the green bar");
        const int innerW = innerRight - innerLeft + 1;
        QVERIFY2(green * 10 >= innerW * 7,
                 qPrintable(QStringLiteral("selected row mostly not green: %1/%2")
                                .arg(green).arg(img.width())));
        QVERIFY2(black >= 2, "selected row text not black");
        QVERIFY2(red >= 1, "selected row hotkey not red");
        b.unregisterPopup(view);
        view->hide();
    }

    // The separator is one row of a single black rule spanning the panel
    // inside the inset border (reference screenshot).
    void dropdown_separator_is_full_width_single_line() {
        ShellWindow w;
        w.resize(900, 600);
        ContentBridge b;
        b.setShellWindow(&w);
        QMenu* view = openViewMenu(w, b);
        QVERIFY2(view, "View menu missing");
        QAction* sep = nullptr;
        for (QAction* a : view->actions())
            if (a->isSeparator()) {
                sep = a;
                break;
            }
        QVERIFY2(sep, "View menu has no separator");
        const QImage img = view->grab().toImage();
        const QRect r = view->actionGeometry(sep);
        int bestBlack = 0;
        for (int y = r.top(); y <= r.bottom(); ++y) {
            int black = 0;
            for (int x = 0; x < img.width(); ++x) {
                const QColor c = img.pixelColor(x, y);
                if (c.red() < 40 && c.green() < 40 && c.blue() < 40) ++black;
            }
            bestBlack = qMax(bestBlack, black);
        }
        // p13: the rule runs edge to edge across the area INSIDE the inset
        // border (the border's own vertical strokes are separate ink).
        const int sepInner = img.width()
            - 2 * (BorlandMenu::borderInset() + BorlandMenu::borderWidth()) - 2;
        QVERIFY2(bestBlack >= sepInner - 2,
                 qPrintable(QStringLiteral("separator not full width inside the border: %1/%2 px black")
                                .arg(bestBlack).arg(sepInner)));
        b.unregisterPopup(view);
        view->hide();
    }

    // Spec 7: while a dropdown is open the status line is REPLACED by the
    // context hint for the highlighted item ("F1 Help │ <hint>"), and it
    // returns to the F-key row when the dropdown closes.
    void dropdown_highlight_updates_status_hint() {
        ShellWindow w;
        w.resize(900, 600);
        ContentBridge b;
        b.setShellWindow(&w);
        QMenu* view = openViewMenu(w, b);
        QVERIFY2(view, "View menu missing");
        QAction* zoomIn = findAction(view, QStringLiteral("In"));
        QVERIFY2(zoomIn, "Zoom In missing");
        const QPoint pt = itemCenterInShell(view, zoomIn, &w);
        b.forwardMouseEvent(QEvent::MouseMove, Qt::NoButton, Qt::NoButton,
                            Qt::NoModifier, pt.x(), pt.y());
        QCoreApplication::processEvents();

        FKeyStatusBar* bar = w.fkeyStatusBar();
        QVERIFY(bar);
        QVERIFY2(bar->displayedText().contains(QStringLiteral("F1 Help")),
                 qPrintable(QStringLiteral("status hint missing 'F1 Help': %1")
                                .arg(bar->displayedText())));
        QVERIFY2(bar->displayedText().contains(QStringLiteral("Increase the text size")),
                 qPrintable(QStringLiteral("status hint missing item hint: %1")
                                .arg(bar->displayedText())));

        view->hide();
        QCoreApplication::processEvents();
        // The dropdown closed: the hint steps aside and the COUNT line is
        // restored (the F-key reminder was retired 2026-11-19).
        QVERIFY2(bar->displayedText().contains(QStringLiteral("Word Count")),
                 qPrintable(QStringLiteral("status line did not return to the count line: %1")
                                .arg(bar->displayedText())));
        b.unregisterPopup(view);
    }

    // Spec 6: the dropdown casts a HARD black drop shadow offset (+2 cols,
    // +1 row): 2 cells wide along the right edge starting one row below the
    // top, and 1 row tall along the bottom edge starting 2 cells right of the
    // box's left edge. Drawn into the composited frame by ContentBridge.
    void dropdown_drop_shadow_is_hard_black_offset() {
        ShellWindow w;
        w.resize(900, 600);
        ContentBridge b;
        b.setShellWindow(&w);
        auto* mb = static_cast<MenuBar*>(w.menuBar());
        QVERIFY(mb);
        QMenu* view = mb->menu(QStringLiteral("View"));
        QVERIFY(view);
        view->move(w.pos() + QPoint(100, 40));   // keep the shadow inside the frame
        b.registerPopup(view);
        view->show();
        QCoreApplication::processEvents();
        b.update();
        const QImage f = b.frame();
        QVERIFY(!f.isNull());

        const QFontMetrics fm(view->font());
        const int cellH = fm.height();
        const int cellW = qMax(1, fm.horizontalAdvance(QLatin1Char('M')));
        const QRect r(view->pos() - w.pos(), view->size());
        const auto isBlack = [](const QColor& c) {
            return c.red() < 40 && c.green() < 40 && c.blue() < 40;
        };

        // Right band: 2 cells wide, starting one row below the popup top.
        int rightBlack = 0, rightTotal = 0;
        for (int y = r.top() + cellH; y <= r.bottom() + cellH; ++y)
            for (int x = r.right() + 1; x <= r.right() + 2 * cellW; ++x) {
                if (x < 0 || y < 0 || x >= f.width() || y >= f.height())
                    continue;
                ++rightTotal;
                if (isBlack(f.pixelColor(x, y)))
                    ++rightBlack;
            }
        QVERIFY2(rightTotal > 0 && rightBlack * 10 >= rightTotal * 8,
                 qPrintable(QStringLiteral("right drop shadow missing: %1/%2 black")
                                .arg(rightBlack).arg(rightTotal)));

        // Bottom band: 1 row tall, starting 2 cells right of the popup left.
        int botBlack = 0, botTotal = 0;
        for (int y = r.bottom() + 1; y <= r.bottom() + cellH; ++y)
            for (int x = r.left() + 2 * cellW; x <= r.right() + 2 * cellW; ++x) {
                if (x < 0 || y < 0 || x >= f.width() || y >= f.height())
                    continue;
                ++botTotal;
                if (isBlack(f.pixelColor(x, y)))
                    ++botBlack;
            }
        QVERIFY2(botTotal > 0 && botBlack * 10 >= botTotal * 8,
                 qPrintable(QStringLiteral("bottom drop shadow missing: %1/%2 black")
                                .arg(botBlack).arg(botTotal)));

        // The shadow starts BELOW the popup top: the cell right of the top row
        // is still shell background, not shadow.
        QVERIFY2(!isBlack(f.pixelColor(r.right() + 1, r.top() + 1)),
                 "drop shadow must start one row below the popup top");
        b.unregisterPopup(view);
        view->hide();
    }
    // Reference screenshot: the dropdown's single black border sits INSIDE the
    // panel - gray shows outside it on every side - with a little padding
    // between the border and the item text.
    void dropdown_border_is_inset_inside_the_panel()
    {
        BorlandMenu m;
        m.addAction(QStringLiteral("Zoom &In"));
        m.addAction(QStringLiteral("Zoom &Out"));
        m.resize(240, 90);
        const QImage img = m.grab().toImage().convertToFormat(QImage::Format_RGB32);

        auto isBlack = [](const QColor& c) {
            return c.red() < 0x30 && c.green() < 0x30 && c.blue() < 0x30;
        };
        auto isGray = [](const QColor& c) {
            return qAbs(c.red() - 0xAA) < 14 && qAbs(c.green() - 0xAA) < 14
                && qAbs(c.blue() - 0xAA) < 14;
        };

        // Panel edge must be gray (the border is not flush with it).
        QVERIFY2(isGray(img.pixelColor(0, img.height() / 2)),
                 "panel edge must be gray, not the black border");

        // Walk in from the left edge: the first black pixel is the border and
        // it must be a few px in.
        int borderX = -1;
        for (int x = 0; x < img.width() / 3; ++x)
            if (isBlack(img.pixelColor(x, img.height() / 2))) {
                borderX = x;
                break;
            }
        QVERIFY2(borderX >= 8,
                 qPrintable(QStringLiteral("black border must be inset well inside the panel "
                                           "(found at x=%1)").arg(borderX)));
        // The stroke is 2px: the pixel after it must still be black, and only
        // then does the padding (gray) resume.
        const int bw = BorlandMenu::borderWidth();
        QVERIFY2(bw >= 2, "border stroke should be at least 2px");
        for (int i = 1; i < bw; ++i)
            QVERIFY2(isBlack(img.pixelColor(borderX + i, img.height() / 2)),
                     qPrintable(QStringLiteral("border stroke thinner than %1px (x=%2)")
                                    .arg(bw).arg(borderX + i)));
        QVERIFY2(isGray(img.pixelColor(borderX + bw + 1, img.height() / 2)),
                 "expected padding between the border and the item text");
    }

    // Right-clicking in the editor opens a context menu carrying the edit
    // commands, styled like the dropdowns. Styled like the dropdowns means it
    // must be a BorlandMenu (borland.qss carries no QMenu rules on purpose, and
    // QStyleSheetStyle hijacks CE_MenuItem as soon as any rule matches), and it
    // must be registered with ContentBridge so it gets the drop shadow and
    // synthesized mouse events.
    //
    // The spellcheck toggle is NOT here any more (user, 2026-11-17): "modify the
    // right click menu to remove the enable spell beck option sice that's in the
    // menu bar". It stays in the Edit menu; the context menu keeps two
    // separators (after Undo/Redo and after Paste) and nothing else.
    void editor_context_menu_offers_only_edit_commands()
    {
        ShellWindow shell;
        ContentBridge b;
        b.setShellWindow(&shell);
        shell.show();
        QCoreApplication::processEvents();

        shell.showEditorContextMenu(QPoint(60, 60));
        QCoreApplication::processEvents();

        auto* menu = shell.findChild<BorlandMenu*>(QStringLiteral("editorContextMenu"));
        QVERIFY2(menu, "editor context menu missing or not a BorlandMenu");

        // The app registers popups on Show (PopupRegistrar in main.cpp), which
        // also hardens them for compositing; emulate that here, because an
        // unhardened popup does not stay visible without a native parent.
        b.registerPopup(menu);
        QVERIFY2(b.registeredPopups().contains(menu),
                 "context menu was not registered with the bridge");
        QVERIFY2(menu->isVisible(), "editor context menu did not open");

        QStringList have;
        for (QAction* a : menu->actions()) {
            if (a->isSeparator())
                continue;
            have << a->objectName();
        }
        const QStringList wanted{
            QStringLiteral("actionEditUndo"), QStringLiteral("actionEditRedo"),
            QStringLiteral("actionEditCut"), QStringLiteral("actionEditCopy"),
            QStringLiteral("actionEditPaste")};
        for (const QString& name : wanted)
            QVERIFY2(have.contains(name),
                     qPrintable(QStringLiteral("context menu missing %1 (has: %2)")
                                    .arg(name, have.join(QStringLiteral(", ")))));

        // The spellcheck toggle belongs to the Edit menu bar only.
        QVERIFY2(!have.contains(QStringLiteral("actionEditSpellcheck")),
                 qPrintable(QStringLiteral("context menu still offers the spellcheck toggle (has: %1)")
                                .arg(have.join(QStringLiteral(", ")))));

        // Exactly ONE separator: Undo/Redo --- Cut/Copy/Paste. The old menu had
        // a second one before the spellcheck row; leaving it behind would show
        // a dangling rule at the bottom of the menu.
        int separators = 0;
        QStringList shape;
        for (QAction* a : menu->actions()) {
            if (a->isSeparator())
                ++separators;
            shape << (a->isSeparator() ? QStringLiteral("---") : a->objectName());
        }
        QVERIFY2(separators == 1,
                 qPrintable(QStringLiteral("expected 1 separator, found %1 (shape: %2)")
                                .arg(separators)
                                .arg(shape.join(QStringLiteral(", ")))));

        // It must be registered, or it gets neither the drop shadow nor the
        // synthesized mouse events that drive hover/dismissal.
        b.unregisterPopup(menu);
    }

    // The REAL input path: a right-button press forwarded by the bridge over
    // the editor must open the context menu. The direct
    // showEditorContextMenu() test above cannot catch a dispatch bug - it
    // calls the menu builder directly and never exercises the routing.
    void right_button_press_through_the_bridge_opens_the_context_menu()
    {
        ShellWindow shell;
        ContentBridge b;
        b.setShellWindow(&shell);
        auto* editor = new QWidget;
        editor->setMinimumSize(200, 200);
        shell.setEditorWidget(editor);
        shell.resize(800, 600);
        shell.show();
        QCoreApplication::processEvents();

        QVERIFY2(editor->width() > 0 && editor->height() > 0,
                 "editor widget has no size, cannot aim a click at it");
        const QPoint centre = editor->mapTo(&shell, editor->rect().center());

        b.forwardMouseEvent(QEvent::MouseButtonPress, Qt::RightButton,
                            Qt::RightButton, Qt::NoModifier, centre.x(), centre.y());
        QCoreApplication::processEvents();

        auto* menu = shell.findChild<BorlandMenu*>(QStringLiteral("editorContextMenu"));
        QVERIFY2(menu, "right-click through the bridge did not create the context menu");
        QVERIFY2(menu->isVisible(), "right-click through the bridge did not open the menu");
    }

    // The bridge raises the context menu only for right-clicks inside the
    // editor area; that decision lives in ShellWindow::isEditorArea.
    void right_click_is_routed_only_from_the_editor_area()
    {
        ShellWindow shell;
        auto* editor = new QWidget;
        auto* child = new QWidget(editor);
        shell.setEditorWidget(editor);
        QVERIFY2(shell.isEditorArea(editor),
                 "the editor widget itself must count as the editor area");
        QVERIFY2(shell.isEditorArea(child),
                 "children of the editor must count as the editor area");
        QVERIFY2(!shell.isEditorArea(shell.menuBar()),
                 "the menu bar is not the editor area");
        QVERIFY2(!shell.isEditorArea(nullptr), "null is not the editor area");
    }

    // User bug: "none of the popups from the status bar have the green highlight
    // on hover". BorlandMenu paints the highlight correctly in isolation, so the
    // break must be in DELIVERY: the bridge has to hand the synthesized move to
    // the open popup, not to whatever sits under it in the shell.
    void hovering_a_registered_popup_highlights_the_row() {
        ShellWindow w;
        w.resize(800, 600);
        ContentBridge b;
        b.setShellWindow(&w);

        const QColor kSelectGreen(0x00, 0xAA, 0x00);
        auto countGreen = [](const QImage& img) {
            int n = 0;
            for (int y = 0; y < img.height(); ++y)
                for (int x = 0; x < img.width(); ++x) {
                    const QColor c = img.pixelColor(x, y);
                    if (c.green() > 140 && c.red() < 60 && c.blue() < 60)
                        ++n;
                }
            return n;
        };

        // A SHORT menu (the encoding dropdown's shape) and a SCROLLED one (the
        // doc-type list's shape) - the user says neither highlights.
        for (const bool scrolled : {false, true}) {
            BorlandMenu menu;
            const int n = scrolled ? 120 : 4;
            for (int i = 0; i < n; ++i)
                menu.addAction(QStringLiteral("Mode %1").arg(i));
            menu.setMaximumWidth(360);
            if (scrolled)
                menu.setMaximumHeight(300);
            menu.resize(360, scrolled ? 300 : 120);
            menu.move(w.pos().x() + 100, w.pos().y() + 100);
            b.registerPopup(&menu);
            menu.show();
            QCoreApplication::processEvents();
            QVERIFY(menu.isVisible());
            QCOMPARE(menu.scrolls(), scrolled);

            QCOMPARE(countGreen(menu.grab().toImage()), 0);

            // Hover a row, in SHELL coordinates, through the bridge.
            const QPoint inShell = menu.mapTo(&w, QPoint(20, 60));
            b.forwardMouseEvent(QEvent::MouseMove, Qt::NoButton, Qt::NoButton,
                                Qt::NoModifier, inShell.x(), inShell.y());
            QCoreApplication::processEvents();

            QVERIFY2(countGreen(menu.grab().toImage()) > 0,
                     scrolled ? "scrolled popup: hover left no green highlight"
                              : "short popup: hover left no green highlight");

            menu.hide();
            b.unregisterPopup(&menu);
        }
    }

    // The live break behind "none of the popups from the status bar have the
    // green highlight on hover": status-bar dropdowns are parented to the
    // StatusSelectors widget, which sits far from the shell origin.
    // QWidget::mapFrom(parent, pos) walks the PARENT CHAIN, but a Qt::Popup is a
    // window, not a child - so every popup-directed event arrived offset by the
    // parent's position and no row was ever hit. Menu-bar menus are parented at
    // the origin, which is why only the status-bar popups lost their highlight.
    void popup_events_work_when_the_popup_parent_sits_away_from_the_origin() {
        ShellWindow w;
        w.resize(800, 600);
        ContentBridge b;
        b.setShellWindow(&w);

        QWidget host(&w);
        host.setGeometry(500, 400, 200, 40);   // like StatusSelectors in the status row

        BorlandMenu menu(&host);
        for (int i = 0; i < 120; ++i)
            menu.addAction(QStringLiteral("Mode %1").arg(i));
        menu.setMaximumWidth(360);
        menu.setMaximumHeight(300);
        menu.resize(360, 300);
        menu.move(600, 200);
        b.registerPopup(&menu);
        menu.show();
        QCoreApplication::processEvents();
        QVERIFY(menu.isVisible());
        QVERIFY(menu.scrolls());

        auto countGreen = [](const QImage& img) {
            int n = 0;
            for (int y = 0; y < img.height(); ++y)
                for (int x = 0; x < img.width(); ++x) {
                    const QColor c = img.pixelColor(x, y);
                    if (c.green() > 140 && c.red() < 60 && c.blue() < 60)
                        ++n;
                }
            return n;
        };
        QCOMPARE(countGreen(menu.grab().toImage()), 0);

        const QPoint hoverShell = w.mapFromGlobal(menu.mapToGlobal(QPoint(20, 60)));
        b.forwardMouseEvent(QEvent::MouseMove, Qt::NoButton, Qt::NoButton,
                            Qt::NoModifier, hoverShell.x(), hoverShell.y());
        QCoreApplication::processEvents();
        QVERIFY2(countGreen(menu.grab().toImage()) > 0,
                 "hover missed the row: popup events must convert with "
                 "mapFromGlobal, not mapFrom(parent)");

        // The same conversion drives clicks: pressing a row must activate it.
        QAction* fired = nullptr;
        connect(&menu, &QMenu::triggered, &menu, [&fired](QAction* a) { fired = a; });
        b.forwardMouseEvent(QEvent::MouseButtonPress, Qt::LeftButton, Qt::LeftButton,
                            Qt::NoModifier, hoverShell.x(), hoverShell.y());
        QCoreApplication::processEvents();
        QVERIFY2(fired, "press through the bridge did not activate the row");
        b.unregisterPopup(&menu);
    }

    // User bug: "how can i use arrow keys to move the list down". Keys went to
    // the editor (keyFocusWidget) even with a dropdown open, so the arrows never
    // reached the popup. An open popup must own the keyboard.
    void key_target_is_the_open_popup() {
        ShellWindow w;
        w.resize(800, 600);
        ContentBridge b;
        b.setShellWindow(&w);

        BorlandMenu menu;
        for (int i = 0; i < 120; ++i)
            menu.addAction(QStringLiteral("Mode %1").arg(i));
        menu.setMaximumWidth(360);
        menu.setMaximumHeight(300);
        menu.resize(360, 300);
        b.registerPopup(&menu);
        menu.show();
        QCoreApplication::processEvents();
        QVERIFY(menu.isVisible());

        QCOMPARE(b.keyTarget(), static_cast<QWidget*>(&menu));

        menu.hide();
        b.unregisterPopup(&menu);
        QVERIFY2(b.keyTarget() != static_cast<QWidget*>(&menu),
                 "a hidden popup must not keep the keyboard");
    }

    // User bug: "how can i use arrow keys to move the list down". Routing the
    // keys to the popup is only half of it: Down must actually move the
    // selection AND scroll the list, end to end through the bridge.
    void arrow_key_through_the_bridge_scrolls_the_list() {
        ShellWindow w;
        w.resize(800, 600);
        ContentBridge b;
        b.setShellWindow(&w);

        BorlandMenu menu;
        for (int i = 0; i < 120; ++i)
            menu.addAction(QStringLiteral("Mode %1").arg(i));
        menu.setMaximumWidth(360);
        menu.setMaximumHeight(300);
        menu.resize(360, 300);
        b.registerPopup(&menu);
        menu.show();
        QCoreApplication::processEvents();
        QVERIFY(menu.scrolls());
        QCOMPARE(menu.scrollOffset(), 0);

        for (int i = 0; i < 40; ++i)
            b.forwardKeyEvent(QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier, QString());
        QCoreApplication::processEvents();

        QVERIFY2(menu.scrollOffset() > 0,
                 "Down did not scroll the list: the open popup never got the key");

        menu.hide();
        b.unregisterPopup(&menu);
    }

    // User bug: "clicking on the scroll doesn't let me scroll down". The QML
    // input layer forwards press/release/move but no wheel at all, so the menu
    // never received a QWheelEvent. The bridge must forward it to the popup.
    void wheel_over_an_open_popup_scrolls_it() {
        ShellWindow w;
        w.resize(800, 600);
        ContentBridge b;
        b.setShellWindow(&w);

        BorlandMenu menu;
        for (int i = 0; i < 120; ++i)
            menu.addAction(QStringLiteral("Mode %1").arg(i));
        menu.setMaximumWidth(360);
        menu.setMaximumHeight(300);
        menu.resize(360, 300);
        menu.move(w.pos().x() + 100, w.pos().y() + 100);
        b.registerPopup(&menu);
        menu.show();
        QCoreApplication::processEvents();
        QVERIFY(menu.scrolls());
        QCOMPARE(menu.scrollOffset(), 0);

        // A point well inside the popup, in SHELL coordinates.
        const QPoint inShell = menu.mapTo(&w, QPoint(20, 60));
        b.forwardWheelEvent(0, -120, Qt::NoButton, Qt::NoModifier, inShell.x(), inShell.y());
        QVERIFY2(menu.scrollOffset() > 0,
                 qPrintable(QStringLiteral("wheel over the popup left scroll at %1")
                                .arg(menu.scrollOffset())));

        menu.hide();
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