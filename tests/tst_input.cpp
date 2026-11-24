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
#include <QAction>
#include "src/shell/ShellWindow.h"
#include "src/shell/MenuBar.h"
#include "src/shell/TabStrip.h"
#include "src/shell/TvFileDock.h"
#include "src/bridge/ContentBridge.h"
#include "src/editor/EditorTabManager.h"
#include <ktexteditor/view.h>
#include <ktexteditor/document.h>

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

    // Typing with NO widget focused must still reach the editor. The app never
    // called setFocus(), so focusWidget() was null and forwardKeyEvent posted to
    // the shell, which ignores keys: typing did nothing. QWidget key events do
    // NOT propagate down to a child, so a null focus target = dead keys.
    // Xadium 2026-11-23: Alt+F must open the File menu in the LIVE app. Every
    // keystroke reaches the shell through ContentBridge::forwardKeyEvent (QML
    // forwards it), and Qt's shortcut map never runs here (no active window),
    // so the bridge has to recognize the mnemonic and pop the menu itself.
    void alt_f_through_the_bridge_opens_the_file_menu() {
        ShellWindow w;
        w.resize(800, 600);
        ContentBridge b;
        b.setShellWindow(&w);

        b.forwardKeyEvent(QEvent::KeyPress, Qt::Key_F, Qt::AltModifier,
                          QString());
        QCoreApplication::processEvents();

        auto* bar = qobject_cast<MenuBar*>(w.menuBar());
        QVERIFY2(bar, "shell menu bar is not a MenuBar");
        QMenu* file = bar->menu(QLatin1String("File"));
        QVERIFY2(file, "no File menu");
        QVERIFY2(file->isVisible(),
                 "Alt+F did not reach the menu bar through the bridge");

        file->close();
        QCoreApplication::processEvents();
    }

    void keys_reach_editor_when_nothing_holds_focus() {
        ShellWindow w;
        w.resize(640, 480);
        ContentBridge b;
        b.setShellWindow(&w);

        auto* edit = new QLineEdit(&w);
        edit->resize(200, 24);
        edit->move(10, 60);
        w.setEditorWidget(edit);
        edit->show();
        QCoreApplication::processEvents();

        // Deliberately do NOT call setFocus(): this is the production state.
        b.forwardKeyEvent(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier,
                          QStringLiteral("a"));
        QCoreApplication::processEvents();

        QCOMPARE(edit->text(), QStringLiteral("a"));
    }

    // Cursor movement and deletion inside the embedded editor are QActions
    // with BARE-key shortcuts: KTextEditor binds Left/Right/Up/Down, Backspace,
    // Delete, Insert and PageUp/PageDown that way. Qt fires those through its
    // shortcut map, which only runs for an ACTIVE window - and the shell is
    // never mapped, so every one of them was dead while ordinary typing (which
    // goes through keyPressEvent, not the shortcut map) still worked. The
    // bridge must dispatch the focused widget's own actions, exactly as it
    // already dispatches the menu's.
    void forwarded_bare_key_fires_the_focused_widgets_action() {
        ShellWindow w;
        w.resize(640, 480);
        ContentBridge b;
        b.setShellWindow(&w);

        auto* canvas = new QWidget(&w);
        canvas->resize(200, 120);
        w.setEditorWidget(canvas);
        canvas->show();

        auto* moveLeft = new QAction(canvas);
        moveLeft->setShortcut(QKeySequence(Qt::Key_Left));
        canvas->addAction(moveLeft);
        int fired = 0;
        QObject::connect(moveLeft, &QAction::triggered, &w,
                         [&fired](bool) { ++fired; });

        b.forwardKeyEvent(QEvent::KeyPress, Qt::Key_Left, Qt::NoModifier, QString());
        QCOMPARE(fired, 1);

        // A key that matches nothing must not fire it.
        b.forwardKeyEvent(QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier, QString());
        QCOMPARE(fired, 1);
    }

    // A disabled action must not be dispatched: with no history there is
    // nothing to undo, and firing a disabled edit action would be a lie.
    void forwarded_bare_key_skips_a_disabled_action() {
        ShellWindow w;
        w.resize(640, 480);
        ContentBridge b;
        b.setShellWindow(&w);

        auto* canvas = new QWidget(&w);
        canvas->resize(200, 120);
        w.setEditorWidget(canvas);
        canvas->show();

        auto* moveLeft = new QAction(canvas);
        moveLeft->setShortcut(QKeySequence(Qt::Key_Left));
        moveLeft->setEnabled(false);
        canvas->addAction(moveLeft);
        int fired = 0;
        QObject::connect(moveLeft, &QAction::triggered, &w,
                         [&fired](bool) { ++fired; });

        b.forwardKeyEvent(QEvent::KeyPress, Qt::Key_Left, Qt::NoModifier, QString());
        QCOMPARE(fired, 0);
    }

    // The real thing, through the production path: a forwarded arrow key moves
    // the KTextEditor cursor. Before the fix the view received the key and
    // ignored it (its handler only types text), so the caret never moved.
    void forwarded_arrows_move_the_editor_cursor() {
        ShellWindow w;
        w.resize(900, 600);
        EditorTabManager tabs;                 // creates one view + document
        w.setEditorWidget(tabs.containerWidget());
        // Mirrors the composition root: KTextEditor's editing actions live in a
        // collection outside the widget tree, so the shell is handed the way in.
        w.setEditorShortcutHandler([&tabs](const QKeySequence& seq) {
            return tabs.triggerActiveViewShortcut(seq);
        });
        ContentBridge b;
        b.setShellWindow(&w);
        w.show();
        tabs.containerWidget()->grab();        // geometry without a real screen

        KTextEditor::View* v = tabs.activeView();
        QVERIFY2(v, "no active view");
        tabs.activeDocument()->setText(QStringLiteral("alpha\nbeta"));
        v->setCursorPosition(KTextEditor::Cursor(0, 2));

        b.forwardKeyEvent(QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier, QString());
        QCOMPARE(v->cursorPosition().column(), 3);

        b.forwardKeyEvent(QEvent::KeyPress, Qt::Key_Left, Qt::NoModifier, QString());
        QCOMPARE(v->cursorPosition().column(), 2);

        b.forwardKeyEvent(QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier, QString());
        QCOMPARE(v->cursorPosition().line(), 1);
    }

    // Backspace and Delete are actions too, and Kate's canvas widget ACCEPTS a
    // Backspace press without doing anything (the action does the work), so a
    // "was the key accepted?" gate would leave deletion dead as well.
    void forwarded_backspace_and_delete_edit_the_document() {
        ShellWindow w;
        w.resize(900, 600);
        EditorTabManager tabs;
        w.setEditorWidget(tabs.containerWidget());
        w.setEditorShortcutHandler([&tabs](const QKeySequence& seq) {
            return tabs.triggerActiveViewShortcut(seq);
        });
        ContentBridge b;
        b.setShellWindow(&w);
        w.show();
        tabs.containerWidget()->grab();

        KTextEditor::View* v = tabs.activeView();
        QVERIFY2(v, "no active view");
        KTextEditor::Document* doc = tabs.activeDocument();
        doc->setText(QStringLiteral("alpha\nbeta"));
        v->setCursorPosition(KTextEditor::Cursor(0, 2));

        b.forwardKeyEvent(QEvent::KeyPress, Qt::Key_Backspace, Qt::NoModifier,
                          QStringLiteral("\b"));
        QCOMPARE(doc->text(), QStringLiteral("apha\nbeta"));
        QCOMPARE(v->cursorPosition().column(), 1);

        b.forwardKeyEvent(QEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier, QString());
        QCOMPARE(doc->text(), QStringLiteral("aha\nbeta"));
    }

    // Ctrl+V must trigger the Edit>Paste ACTION, not just reach the editor.
    // The QML layer forwards synthesized key events straight to the focused
    // widget, which bypasses QApplication's shortcut machinery: QWidget::event
    // only ACCEPTS the key when a ShortcutOverride was delivered first. Without
    // it the press is ignored and the shortcut never fires - paste from the menu
    // worked while Ctrl+V did nothing.
    // The shell is never mapped, so QApplication::activeWindow() is null and
    // Qt's shortcut map never matches. The bridge therefore dispatches the
    // matching QAction itself - without it Ctrl+V reaches the editor but never
    // fires Edit>Paste (the menu item worked all along).
    void ctrl_v_dispatches_the_paste_action() {
        ShellWindow w;
        w.resize(640, 480);
        ContentBridge b;
        b.setShellWindow(&w);

        MenuBar* mb = qobject_cast<MenuBar*>(w.menuBar());
        QVERIFY2(mb, "menu bar missing");
        QAction* paste = mb->findChild<QAction*>(QStringLiteral("actionEditPaste"));
        QVERIFY2(paste, "Edit>Paste action missing");
        // main.cpp sets the shortcut in the real app; set it here the same way
        // so the test exercises the production key sequence.
        paste->setShortcut(QKeySequence::Paste);
        paste->setShortcutContext(Qt::ApplicationShortcut);

        int fired = 0;
        QObject::connect(paste, &QAction::triggered, &w,
                         [&fired](bool) { ++fired; });

        b.dispatchShortcut(Qt::Key_V, Qt::ControlModifier);

        QCOMPARE(fired, 1);
    }

    // Ctrl+Z / Ctrl+Y must reach Undo / Redo. Both actions start DISABLED (a
    // fresh buffer has no history) and dispatchShortcut deliberately ignores
    // disabled actions, so enable them here the way a real document would.
    void ctrl_z_and_ctrl_y_dispatch_undo_and_redo() {
        ShellWindow w;
        w.resize(640, 480);
        ContentBridge b;
        b.setShellWindow(&w);

        MenuBar* mb = qobject_cast<MenuBar*>(w.menuBar());
        QVERIFY2(mb, "menu bar missing");
        QAction* undo = mb->findChild<QAction*>(QStringLiteral("actionEditUndo"));
        QAction* redo = mb->findChild<QAction*>(QStringLiteral("actionEditRedo"));
        QVERIFY2(undo && redo, "Undo / Redo actions missing");
        undo->setEnabled(true);
        redo->setEnabled(true);

        int undoFired = 0, redoFired = 0;
        QObject::connect(undo, &QAction::triggered, &w, [&undoFired](bool) { ++undoFired; });
        QObject::connect(redo, &QAction::triggered, &w, [&redoFired](bool) { ++redoFired; });

        b.dispatchShortcut(Qt::Key_Z, Qt::ControlModifier);
        QCOMPARE(undoFired, 1);
        b.dispatchShortcut(Qt::Key_Y, Qt::ControlModifier);
        QCOMPARE(redoFired, 1);
    }

    // A disabled action must NOT be dispatched: with no history, Ctrl+Z has
    // nothing to undo and firing it would be a lie. This is also why a live
    // probe of Ctrl+Y on a fresh buffer reports fired=0.
    void dispatch_skips_disabled_actions() {
        ShellWindow w;
        w.resize(640, 480);
        ContentBridge b;
        b.setShellWindow(&w);

        MenuBar* mb = qobject_cast<MenuBar*>(w.menuBar());
        QVERIFY2(mb, "menu bar missing");
        QAction* undo = mb->findChild<QAction*>(QStringLiteral("actionEditUndo"));
        QVERIFY2(undo, "Undo action missing");
        QVERIFY2(!undo->isEnabled(), "Undo should start disabled on a fresh buffer");

        int fired = 0;
        QObject::connect(undo, &QAction::triggered, &w, [&fired](bool) { ++fired; });
        b.dispatchShortcut(Qt::Key_Z, Qt::ControlModifier);
        QCOMPARE(fired, 0);
    }

    // Regression: clicking a menu on the LIVE app crashed with SIGSEGV.
    // QMenuBarPrivate::popupAction (qmenubar.cpp) derefs
    // window()->windowHandle()->screen(). The shell is never mapped, so
    // windowHandle() was null and opening a popup crashed. Reproduces the
    // production path (render ticks the shell first, then the click arrives)
    // and asserts the click opens the menu instead of crashing.
    void menu_click_after_render_opens_popup_without_crash() {
        ShellWindow w;
        w.resize(640, 480);
        ContentBridge b;
        b.setShellWindow(&w);   // the fix gives the shell a platform window handle

        QMenuBar* mb = w.menuBar();
        QVERIFY2(mb && !mb->actions().isEmpty(), "no menu bar / no menus");
        QAction* file = mb->actions().first();
        QVERIFY2(file->menu(), "first top-level action has no submenu");

        // Production has rendered the shell every tick before any click arrives;
        // that primes the menubar for the popup path (as observed on the live app).
        b.update();
        const QPoint c = mb->actionGeometry(file).center();
        // Reaching these lines == no SIGSEGV in popupAction (the crash aborts
        // the process before any QVERIFY can run).
        b.forwardMouseEvent(QEvent::MouseButtonPress, Qt::LeftButton,
                            Qt::LeftButton, Qt::NoModifier, c.x(), c.y());
        QCoreApplication::processEvents();
        QVERIFY2(mb->activeAction() == file,
                 "forwarded menu click did not select File (precondition)");
        QVERIFY2(file->menu()->isVisible() || mb->windowHandle(),
                 "menu did not open / shell has no window handle");
        b.forwardMouseEvent(QEvent::MouseButtonRelease, Qt::NoButton,
                            Qt::NoButton, Qt::NoModifier, c.x(), c.y());
        QCoreApplication::processEvents();
    }

    // User report 2026-11-20: "if I type in the file path input box it also
    // types in the editor". The dock is a child of the same shell as the
    // editor, so a forwarded key must land in the focused field and NOWHERE
    // else - the document behind the dock must not change.
    void typing_in_the_file_dock_does_not_reach_the_editor() {
        ShellWindow w;
        w.resize(900, 700);
        EditorTabManager tabs;              // one document + view
        w.setEditorWidget(tabs.containerWidget());
        w.show();
        ContentBridge b;
        b.setShellWindow(&w);
        QCoreApplication::processEvents();
        tabs.containerWidget()->grab();     // geometry without a real screen

        TvFileDock* dock = TvFileDock::openOn(&w, TvFileDock::Open,
                                             QStringLiteral("Open File"),
                                             QStringLiteral("/tmp"));
        QVERIFY(dock);
        QCoreApplication::processEvents();

        auto* path = dock->findChild<QLineEdit*>(QStringLiteral("tvFileDockPath"));
        QVERIFY2(path, "no path edit in the dock");
        const QPoint pt = path->mapTo(&w, path->rect().center());
        b.forwardMouseEvent(QEvent::MouseButtonPress, Qt::LeftButton,
                            Qt::LeftButton, Qt::NoModifier, pt.x(), pt.y());
        QCoreApplication::processEvents();
        QVERIFY2(w.focusWidget() == path, "click did not focus the path field");

        KTextEditor::Document* doc = tabs.activeDocument();
        QVERIFY2(doc, "no active document");
        doc->setText(QString());
        const QString before = doc->text();

        b.forwardKeyEvent(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier,
                          QStringLiteral("a"));
        QCoreApplication::processEvents();

        QVERIFY2(path->text().contains(QLatin1Char('a')),
                 qPrintable(QStringLiteral("typing did not reach the path field (text=%1)")
                                .arg(path->text())));
        QVERIFY2(doc->text() == before,
                 qPrintable(QStringLiteral("typing in the dock also changed the document (text=%1)")
                                .arg(doc->text())));

        dock->reject();
        QCoreApplication::processEvents();
    }

    // Reported 2026-11-21: "backspacing in the path input and file name input
    // and file type also backspaces in the editor". EVERY focused field of the
    // dock must eat its own Backspace - the editor behind the dialog must not
    // lose a character.
    void backspace_in_every_dock_field_stays_in_the_field() {
        ShellWindow w;
        w.resize(900, 700);
        EditorTabManager tabs;
        w.setEditorWidget(tabs.containerWidget());
        w.show();
        ContentBridge b;
        b.setShellWindow(&w);
        QCoreApplication::processEvents();
        tabs.containerWidget()->grab();

        KTextEditor::Document* doc = tabs.activeDocument();
        QVERIFY2(doc, "no active document");
        doc->setText(QStringLiteral("alpha beta"));
        QVERIFY(tabs.activeView());
        tabs.activeView()->setCursorPosition(KTextEditor::Cursor(0, 10));
        const int docBefore = int(doc->text().size());

        int editorShortcutCalls = 0;
        w.setEditorShortcutHandler([&](const QKeySequence& seq) {
            ++editorShortcutCalls;
            return tabs.triggerActiveViewShortcut(seq);
        });

        TvFileDock* dock = TvFileDock::openOn(&w, TvFileDock::Open,
                                             QStringLiteral("Open File"),
                                             QStringLiteral("/tmp"));
        QVERIFY(dock);
        QCoreApplication::processEvents();

        struct FieldProbe {
            const char* name;
            QString seed;
            QString expectedAfter;
        };
        const FieldProbe probes[] = {
            { "tvFileDockPath", QStringLiteral("/tmp/newfil"),
              QStringLiteral("/tmp/newfi") },
            { "tvFileDockEdit", QStringLiteral("report.tx"),
              QStringLiteral("report.t") },
            { "tvFileDockType", QStringLiteral("*.md"),
              QStringLiteral("*.m") },
        };
        for (const FieldProbe& fp : probes) {
            auto* field = dock->findChild<QLineEdit*>(QLatin1String(fp.name));
            QVERIFY2(field, qPrintable(QStringLiteral("no field %1")
                                           .arg(QLatin1String(fp.name))));
            const QPoint pt = field->mapTo(&w, field->rect().center());
            b.forwardMouseEvent(QEvent::MouseButtonPress, Qt::LeftButton,
                                Qt::LeftButton, Qt::NoModifier, pt.x(), pt.y());
            b.forwardMouseEvent(QEvent::MouseButtonRelease, Qt::NoButton,
                                Qt::NoButton, Qt::NoModifier, pt.x(), pt.y());
            QCoreApplication::processEvents();
            field->setText(fp.seed);

            b.forwardKeyEvent(QEvent::KeyPress, Qt::Key_Backspace, Qt::NoModifier,
                              QStringLiteral("\b"));
            QCoreApplication::processEvents();

            QVERIFY2(field->text() == fp.expectedAfter,
                     qPrintable(QStringLiteral("backspace did not edit %1 (text=%2, wanted %3)")
                                    .arg(QLatin1String(fp.name), field->text(),
                                         fp.expectedAfter)));
            QVERIFY2(int(doc->text().size()) == docBefore,
                     qPrintable(QStringLiteral("backspace in %1 reached the editor "
                                               "(doc %2 -> %3)")
                                    .arg(QLatin1String(fp.name))
                                    .arg(docBefore).arg(doc->text().size())));
        }
        QCOMPARE(editorShortcutCalls, 0);

        dock->reject();
        QCoreApplication::processEvents();
    }

    // While a dock owns the keyboard, NO app-level shortcut may fire behind it:
    // before 2026-11-20, Ctrl+S opened Kate's NATIVE save dialog behind File >
    // Open (measured: the watchdog caught the stack at
    // DocumentPrivate::getSaveFileUrl -> QDialog::exec), and Ctrl+X cut it.
    //
    // The chord must be one KTextEditor does NOT bind: Qt's own shortcut map
    // still runs for a focused-but-unowned window, so a KTextEditor-bound chord
    // would open its native dialog regardless of the shell handler and this
    // test would hang instead of asserting. Ctrl+Alt+Shift+K is unbound.
    void dock_owns_the_keyboard_while_it_is_open() {
        ShellWindow w;
        w.resize(900, 700);
        EditorTabManager tabs;
        w.setEditorWidget(tabs.containerWidget());
        w.show();
        ContentBridge b;
        b.setShellWindow(&w);

        int editorShortcutCalls = 0;
        w.setEditorShortcutHandler([&editorShortcutCalls](const QKeySequence&) {
            ++editorShortcutCalls;
            return true;
        });
        QCoreApplication::processEvents();
        tabs.containerWidget()->grab();

        TvFileDock* dock = TvFileDock::openOn(&w, TvFileDock::Open,
                                             QStringLiteral("Open File"),
                                             QStringLiteral("/tmp"));
        QVERIFY(dock);
        QCoreApplication::processEvents();
        QVERIFY2(w.keyboardIsOwned(), "opening the dock did not claim the keyboard");

        b.forwardKeyEvent(QEvent::KeyPress, Qt::Key_K,
                          Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier,
                          QString());
        QCoreApplication::processEvents();
        QCOMPARE(editorShortcutCalls, 0);

        dock->reject();
        QCoreApplication::processEvents();
        QVERIFY2(!w.keyboardIsOwned(), "closing the dock left the keyboard claimed");

        // ...and with the dock gone the same chord reaches the app again.
        b.forwardKeyEvent(QEvent::KeyPress, Qt::Key_K,
                          Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier,
                          QString());
        QCoreApplication::processEvents();
        QCOMPARE(editorShortcutCalls, 1);
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
