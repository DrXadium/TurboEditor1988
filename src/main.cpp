// SPDX-License-Identifier: GPL-3.0-or-later
// CRT-Kate entry: Borland ShellWindow rendered offscreen by ContentBridge
// into the QML CRT scene (EffectChain), which is the visible window —
// CRT effects cover the entire display per the design spec.
#include <QApplication>
#include <QMessageBox>
#include <QMenu>
#include <QMenuBar>
#include <QAction>
#include <QKeySequence>
#include <QEvent>
#include <QKeyEvent>
#include <QInputDialog>
#include <QLineEdit>
#include <QDir>
#include <QFileInfo>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickItem>
#include <QtQuick/qquickitemgrabresult.h>
#include <QTimer>
#include <memory>
#include "src/shell/ShellWindow.h"
#include "src/AppIdentity.h"
#include "src/shell/TabStrip.h"
#include "src/shell/TabListMenu.h"
#include "src/shell/MenuBar.h"
#include "src/shell/RightScrollBar.h"
#include "src/shell/Minimap.h"
#include "src/shell/BottomBand.h"
#include "src/shell/FKeyStatusBar.h"
#include "src/shell/TvFileDock.h"
#include "src/shell/RecentFilesStore.h"
#include <QStandardPaths>
#include "src/shell/BorlandMenu.h"
#include "src/bridge/ContentBridge.h"
#include "src/editor/EditorTabManager.h"
#include "src/editor/StatusSelectors.h"
#include "src/settings/CrtSettings.h"
#include "src/settings/SettingsDialog.h"
#include "src/shell/AboutModal.h"
#include "src/editor/FindReplaceDialog.h"
#include "src/shell/TvModal.h"
#include <QPushButton>
#include "src/session/SessionManager.h"
#include "src/autosave/AutosaveService.h"
#include <ktexteditor/view.h>
#include <ktexteditor/document.h>
#include <sonnet/speller.h>
#include <cstdio>

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    // User-facing identity. The on-disk identity is deliberately different
    // (AppIdentity::storageDirName) so saved sessions/settings are not orphaned
    // by the rename - see SessionManager and CrtSettings.
    app.setApplicationName(AppIdentity::displayName());
    app.setApplicationDisplayName(AppIdentity::displayName());

    // The Borland chrome, rendered offscreen (never mapped to a real window).
    ShellWindow shell;
    shell.resize(1280, 800);
    // Dev aid (unset in normal runs): CRT_KATE_SIZE=<W>x<H> overrides the
    // window size, so paired captures can verify layout at two sizes (Task 5).
    // Parsed here but applied AFTER engine.load(): the QML Window drives the
    // shell size via ContentBridge::resizeShell, so resizing the widget alone
    // would be overwritten the moment QML syncs.
    int forcedW = 0, forcedH = 0;
    if (const QByteArray sz = qgetenv("CRT_KATE_SIZE"); !sz.isEmpty()) {
        const auto parts = QString::fromUtf8(sz).split(QLatin1Char('x'));
        if (parts.size() == 2) {
            bool okw = false, okh = false;
            const int w = parts.at(0).toInt(&okw);
            const int h = parts.at(1).toInt(&okh);
            if (okw && okh && w > 0 && h > 0) {
                forcedW = w;
                forcedH = h;
            }
        }
    }

    // Composition root: host the KTextEditor tab container below the TabStrip,
    // sync the TabStrip to the documents, and install zoom/tools actions.
    EditorTabManager tabs;
    shell.setEditorWidget(tabs.containerWidget());
    tabs.installActions(&shell);
    // Editor key commands (arrows, Backspace, Delete, PageUp/PageDown...) are
    // QActions in KTextEditor's action collection, which is NOT parented into
    // the widget tree and so is unreachable from the bridge's widget walk.
    // Hand the bridge a way in (crt-shell does not link KTextEditor).
    shell.setEditorShortcutHandler([&tabs](const QKeySequence& seq) {
        return tabs.triggerActiveViewShortcut(seq);
    });
    // Spell section (GUI pass, Task 3). crt-shell never links KTextEditor, so
    // the two document-touching jobs live HERE, in the composition root:
    //   1. resolve the word under a right-click (for the bold headline);
    //   2. perform the replace / ignore the popout asks for.
    //
    // Resolve: map the shell-local click into the view's text position, then
    // grow left/right over letters/apostrophes - the same run Kate's own
    // spellchecker marks.
    shell.setSpellWordResolver([&tabs, &shell](const QPoint& shellLocal) -> SpellWord {
        KTextEditor::View* v = tabs.activeView();
        KTextEditor::Document* d = v ? v->document() : nullptr;
        if (!v || !d)
            return SpellWord();
        const QPoint inView = v->mapFromGlobal(shell.mapToGlobal(shellLocal));
        const KTextEditor::Cursor cur = v->coordinatesToCursor(inView);
        if (!cur.isValid())
            return SpellWord();
        const QString line = d->line(cur.line());
        auto isWordChar = [](QChar c) {
            return c.isLetterOrNumber() || c == QLatin1Char('\'');
        };
        int start = cur.column();
        int end = cur.column();
        while (start > 0 && isWordChar(line.at(start - 1)))
            --start;
        while (end < line.size() && isWordChar(line.at(end)))
            ++end;
        if (start >= end)
            return SpellWord();
        return SpellWord{ line.mid(start, end - start), cur.line(), start, end };
    });
    // A picked suggestion replaces the right-clicked range.
    QObject::connect(&shell, &ShellWindow::spellReplacementRequested, &shell,
                     [&tabs](int line, int startCol, int endCol,
                             const QString& repl) {
                         if (KTextEditor::Document* d = tabs.activeDocument())
                             d->replaceText(KTextEditor::Range(line, startCol,
                                                               line, endCol),
                                            repl);
                     });
    // "Ignore word": Sonnet's own session-only ignore - the word is accepted
    // for THIS run without being written to the user's personal dictionary, so
    // a typo the user waved off does not silently become "correct" later.
    QObject::connect(&shell, &ShellWindow::spellIgnoreRequested, &shell,
                     [](int, int, const QString& word) {
                         Sonnet::Speller sp;
                         sp.addToSession(word);
                     });
    // Bind the painted right/bottom scrollbars to the CURRENT view's real
    // scrollbars. The original bug: the document scrollbar was identified by
    // scanning children for the first vertical one, which latched an empty
    // placeholder (painted bar 0..0 while the view was 0..366). crt-shell does
    // not link KTextEditor, so the view lookup lives here.
    //
    // TWO triggers are needed, not one:
    //  - at construction an empty document has no scroll range and the view
    //    has no real KateScrollBar child yet, so the bind must re-run when the
    //    document's text arrives;
    //  - a tab change swaps which view is current.
    QMetaObject::Connection docTextConn;
    auto wireScrollbars = [&shell, &tabs, &docTextConn]() {
        KTextEditor::View* v = tabs.activeView();
        if (!v) {
            shell.wireScrollbars(nullptr, nullptr);
            return;
        }
        shell.wireScrollbars(v->verticalScrollBar(), v->horizontalScrollBar(),
                             static_cast<QWidget*>(v));
        // Re-bind once the active document grows: that is when the view's real
        // scroll range appears. Reconnecting is idempotent, and wireScrollbars
        // is a no-op when the same scrollbar is already bound.
        if (KTextEditor::Document* d = v->document()) {
            QObject::disconnect(docTextConn);
            docTextConn = QObject::connect(
                d, &KTextEditor::Document::textChanged, &shell,
                [&shell, &tabs]() {
                    if (KTextEditor::View* vv = tabs.activeView())
                        shell.wireScrollbars(vv->verticalScrollBar(),
                                             vv->horizontalScrollBar(),
                                             static_cast<QWidget*>(vv));
                });
        }
    };
    wireScrollbars();
    QTimer::singleShot(0, &shell, wireScrollbars);
    QObject::connect(&tabs, &EditorTabManager::tabsChanged, &shell,
                     [&shell](const QStringList& titles, int active) {
                         if (TabStrip* strip = shell.tabStrip())
                             strip->setTabs(titles, active);
                         // The document's top border row (╔ ═ name ═ ╗) names
                         // the ACTIVE document, so it tracks the same signal
                         // the tab row does.
                         shell.setEditorTitle(titles.value(active));
                     });
    QObject::connect(shell.tabStrip(), &TabStrip::tabActivated, &tabs,
                     &EditorTabManager::setActiveIndex);
    // Task 21 (mock): the tab row's other controls act on the documents behind
    // it - + opens one, the chip's green square closes that tab, and ▼ lists
    // every open tab in the same dropdown style as the menu bar.
    if (TabStrip* strip = shell.tabStrip()) {
        QObject::connect(strip, &TabStrip::newTabRequested, &tabs,
                         &EditorTabManager::newDocument);
        QObject::connect(strip, &TabStrip::tabClosed, &tabs, &EditorTabManager::closeTab);
        QObject::connect(strip, &TabStrip::tabListRequested, &shell,
                         [&shell, &tabs]() {
            TabStrip* s = shell.tabStrip();
            if (!s)
                return;
            QMenu* menu = TabListMenu::build(s, s->titles(), s->activeIndex());
            const QRect cell = s->tabListRect();
            // popup(), never exec(): the shell is never mapped (WA_DontShowOnScreen),
            // so exec()'s nested event loop blocks the GUI thread forever - the
            // reported "clicking ▼ breaks the layout" was a frozen shell. The
            // bridge composites the popup and drives it with synthesized events,
            // exactly like the menu bar's dropdowns.
            QObject::connect(menu, &QMenu::triggered, &tabs, [&tabs](QAction* a) {
                const int idx = a ? a->data().toInt() : -1;
                if (idx >= 0)
                    tabs.setActiveIndex(idx);
            });
            QObject::connect(menu, &QMenu::aboutToHide, menu, &QObject::deleteLater);
            // Drop it from the ▼ cell, below the row - and keep the whole list
            // INSIDE the window. Anchoring the left edge at the cell pushed a
            // 163 px menu off the right edge of a 1280 px window (only 8 px of
            // it could ever be composited), which is why the dropdown looked
            // like it never opened. Right-align to the cell, then clamp.
            const QRect shellRect(shell.mapToGlobal(QPoint(0, 0)), shell.size());
            const QRect cellGlobal(s->mapToGlobal(cell.topLeft()), cell.size());
            menu->popup(TabListMenu::anchoredPos(cellGlobal, menu->sizeHint(), shellRect));
        });
    }
    // The active tab change swaps which KTextEditor::View is current, so the
    // painted scrollbars must re-bind to that view's scrollbars. Without this
    // the right/bottom bars kept pointing at the first tab's scrollbars after
    // a tab switch.
    QObject::connect(&tabs, &EditorTabManager::tabsChanged, &shell,
                     [wireScrollbars](const QStringList&, int) { wireScrollbars(); });
    // Bottom band cursor position "L:C" (spec 6): the ACTIVE view's cursor,
    // 1-based. The manager owns which view is active and re-binds on every tab
    // change; connecting the FIRST view here left the indicator frozen at 1:1
    // as soon as a restored session opened more than one file. The manager
    // emits its first position before this connection exists, hence the seed.
    if (BottomBand* band = shell.bottomBand()) {
        auto showCursor = [band](int line, int column) {
            band->setCursorPos(line + 1, column + 1);
        };
        if (KTextEditor::View* v = tabs.activeView()) {
            const KTextEditor::Cursor c = v->cursorPosition();
            showCursor(c.line(), c.column());
        }
        QObject::connect(&tabs, &EditorTabManager::activeCursorPositionChanged,
                         band, showCursor);
    }
    // Status-line selectors (spec 2026-11-14): the four F-row indicators -
    // spell language, encoding, doc type, INS/OVR. Hosted on the right of the
    // FKeyStatusBar. Re-pointed at the active view/document on every tab
    // change (activeViewChanged) so none of them freeze on the first view (the
    // L:C band bug class).
    {
        auto* selectors = new StatusSelectors(&shell);
        static Sonnet::Speller speller;   // owns the dictionary list + current lang
        selectors->setSpeller(&speller);
        // The spell popout must speak the SAME dictionary as the status tag,
        // so a word is never correct in one place and misspelled in another.
        shell.setSpellLanguage(speller.language());
        // Point the indicators at the active view/document now and on change.
        auto repoint = [selectors](KTextEditor::View* v) {
            selectors->setView(v);
            selectors->setDocument(v ? v->document() : nullptr);
            if (v && v->viewMode() == KTextEditor::View::NormalModeOverwrite)
                selectors->setOverwrite(true);
            else
                selectors->setOverwrite(false);
        };
        if (KTextEditor::View* v0 = tabs.activeView())
            repoint(v0);
        // Add to the bar AFTER the first repoint: setStatusSelectors lays the
        // widget out at its sizeHint, which depends on the doc's labels. Adding
        // it first (with a null doc) made the bar reserve the tiny placeholder
        // width and never re-measure once the real text arrived.
        if (FKeyStatusBar* fkeys = shell.fkeyStatusBar())
            fkeys->setStatusSelectors(selectors);
        QObject::connect(&tabs, &EditorTabManager::activeViewChanged,
                         selectors, repoint);
        // INS/OVR click toggles the view's overwrite mode (via Kate's Insert
        // action, which the bridge already dispatches - KTextEditor::View has
        // isOverwriteMode() but no setter; the toggle is Kate's Insert action).
        QObject::connect(selectors, &StatusSelectors::overwriteToggled, &tabs,
                         [&tabs](bool) {
                             // Fire Kate's Insert toggle (shortcut Qt::Key_Insert),
                             // which flips isOverwriteMode() on the active view.
                             tabs.triggerActiveViewShortcut(
                                 QKeySequence(Qt::Key_Insert));
                         });
    }
    // Status-line counts (2026-11-19): the left line shows whole-document word
    // and character counts in place of the retired F-key reminder. Counts come
    // from the SAME document text the user sees, so a 16-character file reads
    // 16; the bar only formats them (FKeyStatusBar::wordCount is the one rule).
    {
        FKeyStatusBar* bar = shell.fkeyStatusBar();
        auto refreshCounts = [&tabs, bar]() {
            if (!bar)
                return;
            KTextEditor::View* v = tabs.activeView();
            KTextEditor::Document* d = v ? v->document() : nullptr;
            if (!d) {
                bar->setCounts(0, 0);
                return;
            }
            const QString text = d->text();
            bar->setCounts(FKeyStatusBar::wordCount(text), text.size());
        };
        // Re-scanning a whole document on every keystroke would stutter on a
        // large file, so edits coalesce through a short single-shot timer.
        // Tab switches bypass it: a stale count on the newly active tab would
        // read as the old tab's numbers (the L:C band bug class).
        auto* debounce = new QTimer(&shell);
        debounce->setSingleShot(true);
        debounce->setInterval(120);
        QObject::connect(debounce, &QTimer::timeout, &shell, refreshCounts);
        // The active DOCUMENT changes with the active view; re-bind its
        // textChanged each time so the count follows the tab you are in.
        auto activeDocConn = std::make_shared<QMetaObject::Connection>();
        auto bindActiveDoc = [&tabs, debounce, activeDocConn]() {
            QObject::disconnect(*activeDocConn);
            KTextEditor::View* v = tabs.activeView();
            KTextEditor::Document* d = v ? v->document() : nullptr;
            if (!d)
                return;
            *activeDocConn = QObject::connect(
                d, &KTextEditor::Document::textChanged, debounce,
                [debounce]() { debounce->start(); });
        };
        QObject::connect(&tabs, &EditorTabManager::activeViewChanged, &shell,
                         [refreshCounts, bindActiveDoc, debounce](
                             KTextEditor::View*) {
                             bindActiveDoc();
                             debounce->stop();   // drop the old tab's pending tick
                             refreshCounts();
                         });
        bindActiveDoc();
        refreshCounts();
    }
    // Seed the strip with the initial tab (created by the manager ctor).
    if (TabStrip* strip0 = shell.tabStrip()) {
        strip0->setTabs(QStringList() << QStringLiteral("untitled"), 0);
        shell.setEditorTitle(QStringLiteral("untitled"));
    }

    // Named sessions (Task 7): snapshot open files + cursor on quit, restore
    // the last session at startup (no-op on a fresh install: nothing saved yet).
    SessionManager sessions;
    // Collects the current tab set for persistence - shared by the quit
    // snapshot AND the Session > Save menu so both record identical state.
    auto collectSessionEntries = [&tabs]() {
        QVector<SessionManager::Entry> entries;
        for (int i = 0; i < tabs.viewCount(); ++i) {
            const QUrl url = tabs.viewFileUrl(i);
            if (url.isEmpty())
                continue;
            const QPoint p = tabs.viewCursorPosition(i);
            entries.append({ url, p.y(), p.x(), tabs.activeIndex() });
        }
        return entries;
    };
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &sessions, [&]() {
        const auto entries = collectSessionEntries();
        if (!entries.isEmpty())
            sessions.setEntries(entries);
    });
    // Opens a restored entry set (files, active tab, cursor) - shared by the
    // launch-time restore AND the Session > Restore Last menu.
    auto restoreEntries = [&tabs](const QVector<SessionManager::Entry>& entries) {
        for (const auto& e : entries)
            tabs.openFile(e.fileUrl);
        if (!entries.isEmpty()) {
            int want = 0;
            for (int i = 0; i < entries.size(); ++i)
                if (entries[i].activeIndex >= 0 && entries[i].activeIndex < entries.size())
                    want = entries[i].activeIndex;
            tabs.setActiveIndex(want);
            const auto& active = entries[want];
            if (KTextEditor::View* v = tabs.activeView())
                v->setCursorPosition(KTextEditor::Cursor(active.line, active.column));
        }
    };
    if (!sessions.lastSession().isEmpty() && sessions.restoreSession(sessions.lastSession()))
        restoreEntries(sessions.entries());

    ContentBridge bridge;
    bridge.setShellWindow(&shell);
    // The spell popout registers itself with the bridge when the section is
    // built (so it composites into the CRT frame like every other menu).
    shell.setSpellPopupHost(&bridge);

    // CRT'd popup menus (Task 9): a QMenu opens as its own native top-level
    // window, which would sit ABOVE the CRT scene. Register it with the bridge
    // so its pixels are composited into the same texture the CRT samples.
    class PopupRegistrar : public QObject {
    public:
        explicit PopupRegistrar(ContentBridge* b, QObject* parent) : QObject(parent), m_b(b) {}
    protected:
        bool eventFilter(QObject* obj, QEvent* ev) override {
            if (auto* menu = qobject_cast<QMenu*>(obj)) {
                if (ev->type() == QEvent::Show)
                    m_b->registerPopup(menu);
                else if (ev->type() == QEvent::Hide)
                    m_b->unregisterPopup(menu);
            }
            return QObject::eventFilter(obj, ev);
        }
    private:
        ContentBridge* m_b;
    };
    auto* registrar = new PopupRegistrar(&bridge, &app);
    app.installEventFilter(registrar);

    // CRT settings (QSettings-backed single source of truth for effect knobs).
    // Loaded at startup; persisted on quit so the dialog's changes survive restart.
    CrtSettings crtSettings;
    const bool settingsLoaded = crtSettings.load();

    // The file dock's recents (MRU-10). Pinned to the LEGACY storage id for the
    // same reason as the sessions and the settings: a rename must not orphan
    // the user's history.
    const QString recentsIni =
        QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
        + QLatin1Char('/') + AppIdentity::storageDirName()
        + QStringLiteral("/recents.ini");
    RecentFilesStore recentFiles(recentsIni);

    QObject::connect(&app, &QCoreApplication::aboutToQuit, &crtSettings,
                     [&crtSettings] {
                         crtSettings.restoreDevBrightness();   // don't persist CRT_KATE_BRIGHT
                         crtSettings.save();
                     });

    // Task 12: apply the editor font size (Turbo Vision default 16px,
    // user-changeable via Settings > Editor/Appearance) to every view, now and
    // whenever it changes. Zoom remains a relative offset on top.
    tabs.setFontSize(crtSettings.fontSize());
    QObject::connect(&crtSettings, &CrtSettings::fontSizeChanged, &tabs,
                     [&tabs, &crtSettings] { tabs.setFontSize(crtSettings.fontSize()); });

    // Task 10: wire Kate-option menus (MenuBar builds the structure and the
    // Borland palette; here the actions get their real features). The session
    // collect/restore helpers above are reused so menu, quit and launch paths
    // record/restore identical state.
    if (auto* menuBar = shell.findChild<MenuBar*>()) {
        auto act = [menuBar](const char* name) -> QAction* {
            return menuBar->findChild<QAction*>(QLatin1String(name));
        };
        auto on = [](QAction* a, auto fn) {
            if (a)
                QObject::connect(a, &QAction::triggered, a,
                                 [fn = std::move(fn)](bool) { fn(); });
        };

        // File
        on(act("actionFileNew"), [&tabs] { tabs.newDocument(); });
        // File > Open / Save As use the app's OWN Turbo Vision dock, drawn as a
        // child of the shell so it lands inside the CRT texture. A native
        // QFileDialog opens a host-themed top-level window above the CRT scene,
        // which is exactly what the user asked to get rid of.
        on(act("actionFileOpen"), [&shell, &tabs, &recentFiles] {
            const QString f = TvFileDock::getOpenFileName(
                &shell, QObject::tr("Open File"),
                QDir::currentPath(), QStringLiteral("All files (*)"),
                recentFiles.entries());
            if (!f.isEmpty()) {
                tabs.openFile(QUrl::fromLocalFile(f));
                recentFiles.add(f);
            }
        });
        on(act("actionFileSave"), [&tabs] { tabs.saveActive(); });
        on(act("actionFileSaveAs"), [&shell, &tabs, &recentFiles] {
            QString start = QDir::currentPath();
            if (auto* d = tabs.activeDocument())
                if (!d->url().isEmpty())
                    start = QFileInfo(d->url().toLocalFile()).absolutePath();
            const QString f = TvFileDock::getSaveFileName(
                &shell, QObject::tr("Save File As"), start,
                QStringLiteral("All files (*)"), recentFiles.entries());
            if (!f.isEmpty()) {
                tabs.saveActiveAs(QUrl::fromLocalFile(f));
                recentFiles.add(f);
            }
        });

        // Edit: undo / redo. They are enabled from the manager's history signal
        // so a fresh buffer offers them greyed out.
        // MenuBar owns the shortcut text; the root only needs the context, so
        // the combos fire from the unmapped QML window too.
        if (QAction* a = act("actionEditUndo")) {
            on(a, [&tabs] { tabs.undo(); });
            a->setShortcutContext(Qt::ApplicationShortcut);
        }
        if (QAction* a = act("actionEditRedo")) {
            on(a, [&tabs] { tabs.redo(); });
            a->setShortcutContext(Qt::ApplicationShortcut);
        }
        // Captures menuBar by value: the local `act` lambda would dangle once
        // this scope ends, while these connections outlive it.
        const auto syncHistory = [menuBar, &tabs]() {
            if (QAction* u = menuBar->findChild<QAction*>(QStringLiteral("actionEditUndo")))
                u->setEnabled(tabs.canUndo());
            if (QAction* r = menuBar->findChild<QAction*>(QStringLiteral("actionEditRedo")))
                r->setEnabled(tabs.canRedo());
        };
        QObject::connect(&tabs, &EditorTabManager::historyChanged, &shell,
                         [syncHistory](bool, bool) { syncHistory(); });
        QObject::connect(&tabs, &EditorTabManager::tabsChanged, &shell,
                         [syncHistory](const QStringList&, int) { syncHistory(); });
        syncHistory();

        // Edit: standard clipboard block. Shortcuts are bound on the QAction
        // (ApplicationShortcut) so Ctrl+X/C/V work from the QML window too.
        for (auto [name, fn] : { std::pair{ "actionEditCut", &EditorTabManager::cut },
                                 std::pair{ "actionEditCopy", &EditorTabManager::copy },
                                 std::pair{ "actionEditPaste", &EditorTabManager::paste } }) {
            if (QAction* a = act(name)) {
                on(a, [&tabs, fn] { (tabs.*fn)(); });
                a->setShortcutContext(Qt::ApplicationShortcut);
            }
        }
        if (QAction* a = act("actionEditCut"))
            a->setShortcut(QKeySequence::Cut);
        if (QAction* a = act("actionEditCopy"))
            a->setShortcut(QKeySequence::Copy);
        if (QAction* a = act("actionEditPaste"))
            a->setShortcut(QKeySequence::Paste);
        if (qEnvironmentVariableIsSet("CRT_KATE_DIAG")) {
            for (const char* n : { "actionEditCut", "actionEditCopy", "actionEditPaste" }) {
                if (QAction* a = act(n))
                    qWarning("DIAG shortcut %s=%s", n,
                             qPrintable(a->shortcut().toString(QKeySequence::NativeText)));
            }
        }

        // Edit: case/sort tools + on-the-fly spellcheck config toggle
        on(act("actionEditUpper"),
           [&tabs] { tabs.changeCase(EditorTabManager::Upper); });
        on(act("actionEditLower"),
           [&tabs] { tabs.changeCase(EditorTabManager::Lower); });
        on(act("actionEditTitle"),
           [&tabs] { tabs.changeCase(EditorTabManager::Title); });
        on(act("actionEditSort"), [&tabs] { tabs.sortLines(); });
        on(act("actionEditAddEol"), [&tabs, &shell] { tabs.addToLines(true, &shell); });
        on(act("actionEditAddBol"), [&tabs, &shell] { tabs.addToLines(false, &shell); });
        on(act("actionEditRemoveBreaks"), [&tabs] { tabs.removeLineBreaks(); });
        if (QAction* spell = act("actionEditSpellcheck"))
            QObject::connect(spell, &QAction::triggered, spell, [&tabs, spell](bool) {
                if (auto* doc = tabs.activeDocument())
                    doc->setConfigValue(QStringLiteral("on-the-fly-spellcheck"),
                                        spell->isChecked());
            });

        // Search. Find and Replace are house-style modals over the shell, so
        // they composite inside the CRT pass like the file dock.
        on(act("actionSearchFind"), [&tabs, &shell] { tabs.findReplace(&shell, 0); });
        on(act("actionSearchReplace"), [&tabs, &shell] { tabs.findReplace(&shell, 1); });
        on(act("actionSearchFindNext"), [&tabs] { tabs.findNext(); });

        // View: zoom + checkable toggles
        on(act("actionViewZoomIn"), [&tabs] { tabs.setZoom(+1); });
        on(act("actionViewZoomOut"), [&tabs] { tabs.setZoom(-1); });
        on(act("actionViewZoomReset"), [&tabs] { tabs.setZoom(0); });
        if (QAction* mm = act("actionViewMinimap"))
            QObject::connect(mm, &QAction::triggered, mm, [&shell, mm](bool) {
                if (auto* mini = shell.minimap())
                    mini->setVisible(mm->isChecked());
            });
        if (QAction* fold = act("actionViewFoldBar"))
            QObject::connect(fold, &QAction::triggered, fold, [&tabs, fold](bool) {
                if (auto* v = tabs.activeView())
                    v->setConfigValue(QStringLiteral("folding-bar"), fold->isChecked());
            });
        if (QAction* wrap = act("actionViewWordWrap")) {
            QObject::connect(wrap, &QAction::triggered, wrap, [&tabs, wrap](bool) {
                tabs.setWordWrap(wrap->isChecked());
            });
            // Wrap is per view, so the check must follow the tab on screen or
            // the menu lies after every switch.
            QObject::connect(&tabs, &EditorTabManager::activeViewChanged, wrap,
                             [&tabs, wrap] { wrap->setChecked(tabs.wordWrap()); });
        }

        // Session -> SessionManager (reuses collectSessionEntries/restoreEntries)
        on(act("actionSessionSave"),
           [&shell, &sessions, &collectSessionEntries] {
               bool ok = false;
               const QString name = QInputDialog::getText(
                   &shell, QObject::tr("Save Session"),
                   QObject::tr("Session name:"), QLineEdit::Normal,
                   sessions.lastSession(), &ok);
               if (!ok || name.trimmed().isEmpty())
                   return;
               const auto entries = collectSessionEntries();
               if (!entries.isEmpty())
                   sessions.setEntries(entries);
               sessions.saveSession(name);
           });
        on(act("actionSessionRestore"), [&shell, &sessions, &restoreEntries] {
            const QString name = sessions.lastSession();
            if (!name.isEmpty() && sessions.restoreSession(name))
                restoreEntries(sessions.entries());
        });

        // Settings -> SettingsDialog, on the house-style modal. It is a child of
        // the shell, so it composites inside the CRT pass like the file dock;
        // TvModal::exec runs the local round loop (a native exec() on a
        // WA_DontShowOnScreen shell would never return).
        on(act("actionSettingsConfigure"), [&shell, &crtSettings] {
            SettingsDialog dlg(&crtSettings);
            TvModal::exec(&shell, &dlg);
        });

        // Help. About is a house-style modal child of the shell, not a
        // QMessageBox: a native window would land outside the CRT pass.
        on(act("actionHelpAbout"), [&shell] {
            AboutModal about;
            TvModal::exec(&shell, &about);
        });

        // Turbo Vision function keys (Task 12): the status line claims these,
        // so wire them live as extra shortcuts on the existing actions.
        // F2 Save, F3 Open, F5 Zoom reset; F6 next tab + F10 menu added below.
        auto addFKey = [](QAction* a, const char* key) {
            if (!a)
                return;
            QStringList keys;
            const QKeySequence cur = a->shortcut();
            keys << cur.toString();
            keys << QString::fromLatin1(key);
            QList<QKeySequence> seq;
            for (const QString& k : keys)
                if (!k.isEmpty())
                    seq << QKeySequence(k);
            a->setShortcuts(seq);
        };
        addFKey(act("actionFileSave"), "F2");
        addFKey(act("actionFileOpen"), "F3");
        addFKey(act("actionViewZoomReset"), "F5");
    }

    // F6: next tab (cycle). Wired after the action block (needs shell/tabs).
    {
        auto* nextTab = new QAction(&shell);
        nextTab->setObjectName(QStringLiteral("actionF6Next"));
        nextTab->setShortcut(QKeySequence(QStringLiteral("F6")));
        shell.addAction(nextTab);
        QObject::connect(nextTab, &QAction::triggered, nextTab, [&tabs] {
            const int n = tabs.tabCount();
            if (n > 0)
                tabs.setActiveIndex((tabs.activeIndex() + 1) % n);
        });
    }
    // F10: open the menu bar (first menu), the TV "F10 = Menu" convention.
    {
        auto* menuAct = new QAction(&shell);
        menuAct->setObjectName(QStringLiteral("actionF10Menu"));
        menuAct->setShortcut(QKeySequence(QStringLiteral("F10")));
        shell.addAction(menuAct);
        QObject::connect(menuAct, &QAction::triggered, menuAct, [&shell] {
            if (QMenuBar* mb = shell.menuBar()) {
                const auto tops = mb->actions();
                if (!tops.isEmpty())
                    mb->setActiveAction(tops.first());
                if (QMenu* m = mb->actions().first()->menu())
                    m->popup(mb->mapToGlobal(QPoint(0, mb->height())));
            }
        });
    }

    if (!settingsLoaded) {
        // Deferred-minor fix: a failed load is surfaced, not swallowed —
        // defaults are in effect and a corrupt/unreadable store is named.
        QMessageBox::warning(
            &shell, QObject::tr("Settings not loaded"),
            QObject::tr("The settings store at %1 could not be read.\n"
                         "Defaults are in effect; saving will rewrite the file.")
                .arg(crtSettings.storePath()));
    }

    // Crash recovery (Task 8 / final-review fix): snapshots modified buffers
    // periodically; on launch, if a previous run left recovery files, PROMPT TO
    // RESTORE (spec: "if recovery files are newer than their originals, prompt
    // to restore"): Restore reopens each snapshot's content in a new tab,
    // Discard drops the snapshots.
    AutosaveService autosave;
    // Autosave round (approach A): BOTH halves - the modified-buffer snapshots
    // (recovery) and a timestamped session snapshot (keep-last-10, enforced by
    // SessionManager::takeSnapshot) - at the user's configured cadence. The
    // sink keeps crt-autosave free of a SessionManager dependency.
    autosave.setSessionSink([&sessions]() {
        if (!sessions.entries().isEmpty())
            sessions.takeSnapshot();
    });
    autosave.setIntervalSeconds(crtSettings.autosaveIntervalSeconds());
    autosave.start();
    // Live settings: the Settings dialog writes through crtSettings, and the
    // service follows without a restart (setIntervalSeconds applies to a
    // running timer).
    QObject::connect(&crtSettings, &CrtSettings::autosaveIntervalSecondsChanged,
                     &autosave, [&autosave, &crtSettings]() {
                         autosave.setIntervalSeconds(
                             crtSettings.autosaveIntervalSeconds());
                     });
    if (const auto pending = autosave.pendingRecovery(); !pending.isEmpty()) {
        QStringList names;
        for (const auto& p : pending)
            names << (p.originalUrl.isEmpty() ? p.autosavePath
                                              : p.originalUrl.toLocalFile());
        const auto choice = QMessageBox::question(
            &shell, QObject::tr("Restore unsaved work"),
            QObject::tr("%1 autosave file(s) from a previous session were found:\n\n%2\n\n"
                         "Restore them now?")
                .arg(pending.size())
                .arg(names.join(QLatin1Char('\n'))),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        for (const auto& p : pending) {
            if (choice == QMessageBox::Yes) {
                const QString content = autosave.recoveryContent(p.id);
                if (!content.isEmpty()) {
                    tabs.newDocument();
                    if (KTextEditor::View* v = tabs.activeView())
                        if (KTextEditor::Document* doc = v->document())
                            doc->setText(content);
                }
                autosave.acceptRecovery(p.id);
            } else {
                autosave.discardRecovery(p.id);
            }
        }
    }

    QQmlApplicationEngine engine;
    // Locate QML at runtime so the binary is not tied to the build tree:
    // 1. CRT_KATE_QML_DIR env override,
    // 2. installed layout (AppImage: <appdir>/../share/crt-kate),
    // 3. build-time source dir (dev builds).
    const QString qmlRoot = []() -> QString {
        const QString env = qEnvironmentVariable("CRT_KATE_QML_DIR");
        if (!env.isEmpty())
            return env;
        const QString installed =
            QDir::cleanPath(QCoreApplication::applicationDirPath()
                            + QStringLiteral("/../share/crt-kate"));
        if (QFileInfo::exists(installed + QStringLiteral("/src/main.qml")))
            return installed;
        return QStringLiteral(CRT_KATE_SOURCE_DIR);
    }();
    engine.addImportPath(qmlRoot); // `import src`
    engine.addImageProvider(QStringLiteral("bridge"),
                             new BridgeImageProvider(&bridge));
    engine.rootContext()->setContextProperty(QStringLiteral("bridge"), &bridge);
    engine.rootContext()->setContextProperty(QStringLiteral("crtSettings"),
                                             &crtSettings);

    // ~30 fps frame feed (spec's approved perf budget for QWidget::render).
    QTimer frameTimer;
    QObject::connect(&frameTimer, &QTimer::timeout, &bridge, &ContentBridge::update);
    frameTimer.start(33);

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, []() { QCoreApplication::exit(1); },
                     Qt::QueuedConnection);

    // Render the first frame BEFORE the QML Image resolves its provider URL
    // (avoids a null-fetch at frameSerial=0).
    bridge.update();

    engine.load(QUrl::fromLocalFile(qmlRoot + QStringLiteral("/src/main.qml")));
    if (engine.rootObjects().isEmpty())
        return 1;
    // Apply CRT_KATE_SIZE to the QML window (its onWidth/onHeightChanged
    // handlers push the size down to the bridged shell).
    if (forcedW > 0 && forcedH > 0) {
        if (auto* rootWin =
                qobject_cast<QQuickWindow*>(engine.rootObjects().value(0))) {
            rootWin->setWidth(forcedW);
            rootWin->setHeight(forcedH);
        }
    }

    // Dev aid (unset in normal runs): CRT_KATE_MENU=<title> opens a probe menu
    // over the shell before CRT_KATE_SHOT captures, proving a real popup gets
    // registered + composited into the CRT texture (Task 9 E2E). Opened via a
    // queued timer so the PopupRegistrar's event filter sees its Show event.
    if (const QByteArray menuTitle = qgetenv("CRT_KATE_MENU"); !menuTitle.isEmpty()) {
        QTimer::singleShot(700, &app, [&app, menuTitle]() {
            auto* probe = new QMenu(QString::fromUtf8(menuTitle));
            probe->addAction(QStringLiteral("First action"));
            probe->addAction(QStringLiteral("Second action"));
            probe->addAction(QStringLiteral("Third action"));
            probe->setObjectName(QStringLiteral("crtProbeMenu"));
            probe->move(40, 60);           // over the shell's top-left area
            probe->show();
        });
    }

    // Dev aid (unset in normal runs): CRT_KATE_MODAL=about|settings shows the
    // real house-style modal over the shell before CRT_KATE_SHOT captures it.
    // Shown WITHOUT the round loop - exec() would block the capture timer.
    if (const QByteArray which = qgetenv("CRT_KATE_MODAL"); !which.isEmpty()) {
        QTimer::singleShot(700, &app, [&shell, &crtSettings, &tabs, which]() {
            TvModal* modal = nullptr;
            if (which == "about")
                modal = new AboutModal(&shell);
            else if (which == "settings")
                modal = new SettingsDialog(&crtSettings, &shell);
            else if (which == "find" || which == "replace") {
                // Find and Replace, shown WITHOUT the round loop so a capture
                // catches it and a probe can click its plates.
                auto* find = new FindReplaceDialog(&tabs, &shell);
                find->setInitialField(which == "replace" ? 1 : 0);
                find->findField()->setText(QStringLiteral("needle"));
                find->replaceField()->setText(QStringLiteral("pin"));
                modal = find;
            }
            if (!modal)
                return;
            const QSize want = modal->sizeHint();
            modal->resize(want);
            modal->move((shell.width() - want.width()) / 2,
                        (shell.height() - want.height()) / 2);
            modal->show();
            modal->raise();
        });
    }

    // Dev aid (unset in normal runs): opens the REAL dropdown with this title
    // over the shell before a capture, proving the dropdown + drop shadow
    // composite end-to-end and that opening a real menu does not crash (the
    // rev1 menu-click regression). CRT_KATE_VIEWMENU=1 is the historical
    // spelling for View; CRT_KATE_MENUBAR=<title> opens any menu (2026-11-19,
    // used to capture the Edit menu's new line-tools group).
    const QByteArray menubarTitle = qgetenv("CRT_KATE_MENUBAR");
    const bool viewMenuAid = qEnvironmentVariableIsSet("CRT_KATE_VIEWMENU");
    if (viewMenuAid || !menubarTitle.isEmpty()) {
        const QString title = !menubarTitle.isEmpty()
            ? QString::fromUtf8(menubarTitle)
            : QStringLiteral("View");
        QTimer::singleShot(700, &app, [&shell, title]() {
            if (auto* mb = qobject_cast<MenuBar*>(shell.menuBar())) {
                if (QMenu* menu = mb->menu(title)) {
                    // Behave like a real click: mark the bar title selected (so
                    // it paints its green plate) and highlight the first item
                    // (so the dropdown paints its green selection bar).
                    int x = 40;
                    for (QAction* a : mb->actions())
                        if (a->menu() == menu) {
                            mb->setActiveAction(a);
                            x = mb->actionGeometry(a).x();
                            break;
                        }
                    menu->move(shell.pos() + QPoint(x, 60));
                    menu->show();
                    if (!menu->actions().isEmpty())
                        menu->setActiveAction(menu->actions().first());
                }
            }
        });
    }

    // Dev aid (unset in normal runs): CRT_KATE_BRIGHT=<0..3> forces the CRT
    // brightness at launch so a capture can reveal the underlying chrome
    // (used with CRT_KATE_SHOT for visual restyle verification).
    if (const QByteArray b = qgetenv("CRT_KATE_BRIGHT"); !b.isEmpty()) {
        bool ok = false;
        const double v = b.toDouble(&ok);
        if (ok)
            crtSettings.applyDevBrightness(v);
    }

    // Dev aid (unset in normal runs): CRT_KATE_RAWSHOT=/tmp/x.png grabs the
    // RAW widget render (the true chrome, no CRT shader) and exits - used for
    // visual restyle verification / pixel proofs against the Turbo Vision spec.
    // CRT_KATE_RAWSHOT_DELAY (ms) lets the grab wait for a text-seeding dev aid
    // (they run at 1200ms); unset keeps the historical 900ms.
    if (const QByteArray raw = qgetenv("CRT_KATE_RAWSHOT"); !raw.isEmpty()) {
        const int rawDelay = qEnvironmentVariableIntValue("CRT_KATE_RAWSHOT_DELAY");
        QTimer::singleShot(rawDelay > 0 ? rawDelay : 900, &app, [&shell, raw, &app]() {
            const QImage img = shell.grab().toImage();
            img.save(QString::fromUtf8(raw));
            QCoreApplication::exit(0);
        });
    }

    // Dev aid (unset in normal runs): CRT_KATE_KEY=Ctrl+V types that shortcut
    // into the real app through the QML input path and prints whether the
    // associated QAction fired - the only way to verify QML key forwarding
    // without a pointer tool on the host. CRT_KATE_KEY_ACTION names the action
    // to watch (default actionEditPaste), so any binding can be exercised.
    if (const QByteArray keyArg = qgetenv("CRT_KATE_KEY"); !keyArg.isEmpty()) {
        QTimer::singleShot(1200, &app, [&app, keyArg, &shell, &bridge, &tabs]() {
            const QKeySequence seq = QKeySequence::fromString(QString::fromUtf8(keyArg));
            const int key = seq[0].key();
            const auto mods = static_cast<Qt::KeyboardModifiers>(
                static_cast<int>(seq[0].keyboardModifiers()));
            auto* mb = shell.findChild<MenuBar*>();
            const QByteArray watchArg = qgetenv("CRT_KATE_KEY_ACTION");
            const QString watch = watchArg.isEmpty()
                ? QStringLiteral("actionEditPaste")
                : QString::fromUtf8(watchArg);
            QAction* a = mb ? mb->findChild<QAction*>(watch) : nullptr;
            auto* fired = new bool(false);
            if (a)
                QObject::connect(a, &QAction::triggered, &app,
                                 [fired](bool) { *fired = true; });
            // Snapshot the buffer so the log shows what the key actually did -
            // this is what distinguishes "the action fired" from "the document
            // changed the way the user expects".
            auto docText = [&tabs]() -> QString {
                if (auto* d = tabs.activeDocument())
                    return d->text();
                return QString();
            };
            const QString before = docText();
            if (key != Qt::Key_unknown)
                bridge.forwardKeyEvent(QEvent::KeyPress, key, mods, QString());
            QTimer::singleShot(300, &app, [&app, keyArg, watch, fired, a, before, docText]() {
                const QString after = docText();
                qWarning("DIAG key=%s action=%s fired=%d enabled=%d changed=%d lenBefore=%d lenAfter=%d",
                         qPrintable(QString::fromUtf8(keyArg)), qPrintable(watch),
                         int(*fired), a ? int(a->isEnabled()) : -1,
                         int(before != after), int(before.size()), int(after.size()));
                QCoreApplication::exit(0);
            });
        });
    }

    // Dev aid (unset in normal runs): CRT_KATE_RCLICK=1 synthesizes a
    // right-button press at the centre of the editor through the bridge - the
    // SAME call QML makes from onPressed - so the editor context menu can be
    // verified on this host, which has no pointer tool. Combine with
    // CRT_KATE_SHOT to capture the composited window with the menu open.
    if (qEnvironmentVariableIsSet("CRT_KATE_RCLICK")) {
        QTimer::singleShot(1200, &app, [&shell, &bridge]() {
            QWidget* ed = shell.keyFocusWidget();
            if (!ed || ed->width() <= 0 || ed->height() <= 0)
                return;
            const QPoint p = ed->mapTo(&shell, ed->rect().center());
            bridge.forwardMouseEvent(QEvent::MouseButtonPress, Qt::RightButton,
                                     Qt::RightButton, Qt::NoModifier, p.x(), p.y());
        });
    }

    // Dev aid (unset in normal runs): CRT_KATE_SPELLHOVER=1 right-clicks a
    // KNOWN misspelled word in the editor, then hovers the headline row, so
    // the spell popout (suggestions / add / ignore) can be captured with
    // CRT_KATE_SHOT. This host has no pointer tool; the bridge IS the input.
    // Without this the context menu opens over an empty line and no section
    // builds (the word must be under the click).
    if (qEnvironmentVariableIsSet("CRT_KATE_SPELLHOVER")) {
        QTimer::singleShot(700, &app, [&app, &shell, &bridge, &tabs]() {
            // Put a misspelled word at the top of the buffer and aim at it.
            if (KTextEditor::View* v = tabs.activeView()) {
                v->document()->setText(QStringLiteral("reibot second line\n"));
                if (KTextEditor::Document* d = v->document())
                    d->setHighlightingMode(QStringLiteral("Plain Text"));
            }
            QTimer::singleShot(300, &app, [&app, &shell, &bridge, &tabs]() {
                // Use the ACTIVE view, not findChild(): findChild can reach a
                // hidden stacked-child view whose cursorToCoordinate is stale.
                KTextEditor::View* v = tabs.activeView();
                if (!v)
                    return;
                // Word is at line 0, column 3 - aim there, not the view centre
                // (the centre maps to an empty line where no spell section builds).
                const QPoint wordView = v->cursorToCoordinate(KTextEditor::Cursor(0, 3));
                if (wordView.x() < 0)
                    return;
                const QPoint p = v->mapTo(&shell, wordView);
                bridge.forwardMouseEvent(QEvent::MouseButtonPress, Qt::RightButton,
                                         Qt::RightButton, Qt::NoModifier, p.x(), p.y());
                // Hover the headline (the menu's first action) to open the popout.
                QTimer::singleShot(300, &app, [&shell, &bridge]() {
                    auto* menu = shell.findChild<BorlandMenu*>(QStringLiteral("editorContextMenu"));
                    if (!menu || !menu->isVisible())
                        return;
                    bridge.registerPopup(menu);
                    QAction* head = nullptr;
                    for (QAction* a : menu->actions())
                        if (a->objectName() == QLatin1String("spellWord"))
                            head = a;
                    if (!head)
                        return;
                    const QPoint hp = menu->pos() - shell.pos()
                                      + menu->actionGeometry(head).center();
                    bridge.forwardMouseEvent(QEvent::MouseMove, Qt::NoButton,
                                             Qt::NoButton, Qt::NoModifier, hp.x(), hp.y());
                });
            });
        });
    }

    // Dev aid (unset in normal runs): CRT_KATE_SYNTAX=<mode> seeds a small
    // code sample and switches the active document to that highlighting mode,
    // so CRT_KATE_SHOT captures the Borland syntax palette rendering real
    // tokens (keywords cyan, strings green, comments muted, numbers magenta).
    if (const QByteArray mode = qgetenv("CRT_KATE_SYNTAX"); !mode.isEmpty()) {
        QTimer::singleShot(700, &app, [&app, &tabs, mode]() {
            if (KTextEditor::View* v = tabs.activeView()) {
                KTextEditor::Document* d = v->document();
                d->setText(QStringLiteral(
                    "#include <stdio.h>\n"
                    "// a muted comment\n"
                    "int main(void) {\n"
                    "    const char* s = \"green string\";\n"
                    "    int n = 42;\n"
                    "    if (n > 0) printf(\"%s %d\\n\", s, n);\n"
                    "    return 0;\n"
                    "}\n"));
                d->setHighlightingMode(QString::fromUtf8(mode));
            }
            Q_UNUSED(app);
        });
    }

    // Dev aid (unset in normal runs): CRT_KATE_FILEDLG=open|save opens the TV
    // file dock at launch so CRT_KATE_SHOT can capture it composited INSIDE the
    // CRT frame - the proof that the dialog is drawn on the app's own surface
    // rather than in a separate native popup. Non-modal on purpose: the app has
    // to stay alive for the capture.
    if (const QByteArray which = qgetenv("CRT_KATE_FILEDLG"); !which.isEmpty()) {
        const bool save = (which == QByteArrayLiteral("save"));
        QTimer::singleShot(800, &app, [&shell, save, &recentFiles]() {
            TvFileDock::openOn(&shell, save ? TvFileDock::Save : TvFileDock::Open,
                               save ? QObject::tr("Save File As")
                                    : QObject::tr("Open File"),
                               QDir::currentPath(),
                               QStringLiteral("All files (*)"),
                               recentFiles.entries());
        });
    }

    // Dev aid (unset in normal runs): CRT_KATE_CLICKPROBE=dockhelp sends a
    // press+release through the bridge at the dock's Help plate - a plate that
    // deliberately does NOT close its dialog, so a capture taken afterwards
    // shows whether the plate came back to rest. This host has no pointer tool;
    // the bridge IS the input, and the values match QML exactly (press
    // buttons=LeftButton, release buttons=NoButton).
    if (const QByteArray what = qgetenv("CRT_KATE_CLICKPROBE"); !what.isEmpty()) {
        QTimer::singleShot(1600, &app, [&app, &shell, &bridge, what]() {
            // Grab AFTER the release: a capture taken before the click proves
            // nothing (that mistake cost a whole round of false "it reverts").
            auto shotAfterRelease = [&shell, &app]() {
                const QByteArray out = qgetenv("CRT_KATE_CLICKSHOT");
                if (out.isEmpty())
                    return;
                QTimer::singleShot(400, &app, [&shell, &app, out]() {
                    shell.grab().save(QString::fromLocal8Bit(out));
                    QCoreApplication::quit();
                });
            };
            // The modal plate: CRT_KATE_MODAL shows a TvModal WITHOUT the round
            // loop, so clicking OK does not hide it and the capture shows the
            // plate's state right after the release.
            if (what == "modalok") {
                auto* modal = shell.findChild<TvModal*>();
                if (!modal)
                    return;
                auto* plate = modal->findChild<QPushButton*>(QStringLiteral("tvModalButton"));
                if (!plate)
                    return;
                const QPoint p = plate->mapTo(&shell, plate->rect().center());
                bridge.forwardMouseEvent(QEvent::MouseButtonPress, Qt::LeftButton,
                                         Qt::LeftButton, Qt::NoModifier, p.x(), p.y());
                QCoreApplication::processEvents();
                bridge.forwardMouseEvent(QEvent::MouseButtonRelease, Qt::LeftButton,
                                         Qt::NoButton, Qt::NoModifier, p.x(), p.y());
                QCoreApplication::processEvents();
                shotAfterRelease();
                return;
            }
            auto* dock = shell.findChild<TvFileDock*>();
            if (!dock)
                return;
            QRect target;
            if (what == "dockhelp")
                target = dock->helpRect();
            else if (what == "dockok")
                target = dock->okRect();
            else
                return;
            if (target.isNull())
                return;
            const QPoint p = dock->mapTo(&shell, target.center());
            bridge.forwardMouseEvent(QEvent::MouseButtonPress, Qt::LeftButton,
                                     Qt::LeftButton, Qt::NoModifier, p.x(), p.y());
            QCoreApplication::processEvents();
            bridge.forwardMouseEvent(QEvent::MouseButtonRelease, Qt::LeftButton,
                                     Qt::NoButton, Qt::NoModifier, p.x(), p.y());
            QCoreApplication::processEvents();
            shotAfterRelease();
        });
    }

    // Dev aid (unset in normal runs): CRT_KATE_TABLIST=1 clicks the ▼ tab-list
    // control through the bridge - the same path a real click takes - so the
    // dropdown can be captured without a pointer. Synthetic XTEST clicks cannot
    // reach the app's Xwayland surface from the relay shell, so this is the only
    // way to see the list open on screen.
    if (qEnvironmentVariableIsSet("CRT_KATE_TABLIST")) {
        QTimer::singleShot(1200, &app, [&shell, &bridge]() {
            TabStrip* s = shell.tabStrip();
            if (!s || s->width() <= 0)
                return;
            const QPoint p = s->mapTo(&shell, s->tabListRect().center());
            bridge.forwardMouseEvent(QEvent::MouseButtonPress, Qt::LeftButton,
                                     Qt::LeftButton, Qt::NoModifier, p.x(), p.y());
            bridge.forwardMouseEvent(QEvent::MouseButtonRelease, Qt::LeftButton,
                                     Qt::NoButton, Qt::NoModifier, p.x(), p.y());
        });
    }

    // Dev aid (unset in normal runs): CRT_KATE_STATUSHOVER=1 opens the doc-type
    // dropdown through the bridge (the SAME call QML makes from onPressed) and
    // then hovers a row in it, so the highlight can be captured with
    // CRT_KATE_SHOT. This host has no pointer tool, so this is the only way to
    // see hover behave on the real composited popup.
    if (qEnvironmentVariableIsSet("CRT_KATE_STATUSHOVER")) {
        // Open early enough that the hover lands BEFORE CRT_KATE_SHOT captures
        // (the shot fires at 1500 ms and exits).
        QTimer::singleShot(800, &app, [&app, &shell, &bridge]() {
            auto* sel = shell.findChild<StatusSelectors*>();
            if (!sel)
                return;
            const QPoint p = sel->mapTo(&shell, sel->docTypeCenter());
            bridge.forwardMouseEvent(QEvent::MouseButtonPress, Qt::LeftButton,
                                     Qt::LeftButton, Qt::NoModifier, p.x(), p.y());
            bridge.forwardMouseEvent(QEvent::MouseButtonRelease, Qt::LeftButton,
                                     Qt::NoButton, Qt::NoModifier, p.x(), p.y());
            QTimer::singleShot(300, &app, [&shell, &bridge]() {
                for (QWidget* w : bridge.registeredPopups()) {
                    if (!w || !w->isVisible())
                        continue;
                    // Top-level popup: convert through GLOBAL (mapTo(&shell) would
                    // add the popup's pos AND its parent widget's offsets).
                    const QPoint hp = shell.mapFromGlobal(w->mapToGlobal(QPoint(20, 60)));
                    bridge.forwardMouseEvent(QEvent::MouseMove, Qt::NoButton,
                                             Qt::NoButton, Qt::NoModifier, hp.x(), hp.y());
                    break;
                }
            });
        });
    }

    // Dev aid (unset in normal runs): CRT_KATE_SCROLL_PROBE=1 loads a long
    // document, drives the painted right-border bar and reports whether the
    // real KTextEditor view actually scrolled (firstDisplayedLine). This is the
    // only way to tell "signals are connected" apart from "the view scrolls".
    if (qEnvironmentVariableIsSet("CRT_KATE_SCROLL_PROBE")) {
        QTimer::singleShot(1200, &app, [&app, &shell, &tabs]() {
            if (auto* d = tabs.activeDocument()) {
                QStringList lines;
                for (int i = 0; i < 400; ++i)
                    lines << QStringLiteral("line %1").arg(i);
                d->setText(lines.join(QLatin1Char('\n')));
            }
            // No re-wire here on purpose: the probe must measure the SAME
            // wiring path the app uses (wireScrollbars at construction plus
            // on tab change). Re-wiring at probe time would hide a real
            // startup binding bug behind the dev aid.
            QTimer::singleShot(700, &app, [&app, &shell, &tabs]() {
                auto* v = tabs.activeView();
                auto* bar = shell.findChild<RightScrollBar*>(QStringLiteral("rightScrollBar"));
                QStringList bars;
                if (v) {
                    for (QScrollBar* b : v->findChildren<QScrollBar*>())
                        bars << QStringLiteral("%1/%2/%3..%4")
                                    .arg(QString::fromLatin1(b->metaObject()->className()))
                                    .arg(b->orientation() == Qt::Vertical ? "V" : "H")
                                    .arg(b->minimum()).arg(b->maximum());
                }
                const int before = v ? v->firstDisplayedLine() : -1;
                const int barMax = bar ? bar->maximum() : -1;
                if (bar)
                    bar->setValue(barMax / 2);
                QTimer::singleShot(500, &app, [&app, v, bar, before, barMax, bars]() {
                    qWarning("DIAG scroll before=%d after=%d barMax=%d barVal=%d bars=[%s]",
                             before, v ? v->firstDisplayedLine() : -1, barMax,
                             bar ? bar->value() : -1,
                             qPrintable(bars.join(QStringLiteral(", "))));
                    QCoreApplication::exit(0);
                });
            });
        });
    }

    // Dev aid (unset in normal runs): CRT_KATE_KEYPROBE=1 drives the SAME
    // bridge call QML makes for each key and reports what it did to the buffer,
    // the cursor and the bottom band. Synthetic XTEST keys cannot reach the
    // app's surface from the relay shell, so this is the only way to reproduce
    // "arrow keys / backspace do nothing" on this host.
    if (qEnvironmentVariableIsSet("CRT_KATE_KEYPROBE")) {
        QTimer::singleShot(1200, &app, [&app, &shell, &bridge, &tabs]() {
            auto* view = tabs.activeView();
            auto* doc = tabs.activeDocument();
            if (doc)
                doc->setText(QStringLiteral("alpha beta\ngamma delta"));
            if (view)
                view->setCursorPosition(KTextEditor::Cursor(0, 0));
            // Count the cursor moves so the report can show whether each key
            // actually moved the caret (and the band followed).
            auto* moved = new int(0);
            if (view)
                QObject::connect(view, &KTextEditor::View::cursorPositionChanged,
                                 &app, [moved](KTextEditor::View*, const KTextEditor::Cursor&) {
                                     ++*moved;
                                 });
            auto* seen = new int(0);
            if (view) {
                class KeyCounter : public QObject
                {
                public:
                    int* n;
                    bool eventFilter(QObject*, QEvent* ev) override
                    {
                        if (ev->type() == QEvent::KeyPress)
                            ++*n;
                        return false;
                    }
                };
                auto* kc = new KeyCounter;
                kc->n = seen;
                view->installEventFilter(kc);
            }
            auto report = [&tabs, &shell, seen, moved](const char* label) {
                auto* v = tabs.activeView();
                const KTextEditor::Cursor c =
                    v ? v->cursorPosition() : KTextEditor::Cursor(-1, -1);
                QString text = tabs.activeDocument() ? tabs.activeDocument()->text()
                                                     : QString();
                text.replace(QLatin1Char('\n'), QLatin1String("\\n"));
                auto* band = shell.bottomBand();
                std::fprintf(stderr,
                             "DIAG keyprobe %-10s cursor=%d:%d band=%s seen=%d moved=%d views=%d active=%d doc=[%s]\n",
                             label, c.line(), c.column(),
                             band ? band->cursorPosText().toUtf8().constData() : "<none>",
                             *seen, *moved, tabs.viewCount(), tabs.activeIndex(),
                             text.toUtf8().constData());
                std::fflush(stderr);
            };
            report("initial");
            // A real user clicks into the editor before typing. The bridge
            // moves focus on a synthesized press, so reproduce that too: the
            // startup state (nothing focused) is not the state the user is in.
            if (view && view->width() > 0 && view->height() > 0) {
                const QPoint p = view->mapTo(&shell, view->rect().center());
                bridge.forwardMouseEvent(QEvent::MouseButtonPress, Qt::LeftButton,
                                         Qt::LeftButton, Qt::NoModifier, p.x(), p.y());
                bridge.forwardMouseEvent(QEvent::MouseButtonRelease, Qt::NoButton,
                                         Qt::NoButton, Qt::NoModifier, p.x(), p.y());
                report("click");
            }
            struct KeyStep {
                int key;
                const char* label;
                const char* text;
            };
            const KeyStep steps[] = {
                { Qt::Key_Right, "Right", "" },
                { Qt::Key_Right, "Right2", "" },
                { Qt::Key_Down, "Down", "" },
                { Qt::Key_End, "End", "" },
                { Qt::Key_Backspace, "Backspace", "\b" },
                { Qt::Key_Delete, "Delete", "" },
                { Qt::Key_Left, "Left", "" },
                { Qt::Key_A, "A", "A" },
            };
            for (const KeyStep& k : steps) {
                bridge.forwardKeyEvent(QEvent::KeyPress, k.key, Qt::NoModifier,
                                       QString::fromUtf8(k.text));
                bridge.forwardKeyEvent(QEvent::KeyRelease, k.key, Qt::NoModifier,
                                       QString::fromUtf8(k.text));
                report(k.label);
            }
            QTimer::singleShot(300, &app, [&app]() { QCoreApplication::exit(0); });
        });
    }

    // Dev aid (unset in normal runs): CRT_KATE_SHOT=/tmp/x.png grabs the CRT
    // window after 1.5s and exits - used for headless visual verification.
    // Grabs via QQuickItem::grabToImage() on the EffectChain (full-window item).
    // NOTE: do NOT verify renders with QT_QPA_PLATFORM=offscreen — offscreen has
    // no GL, so Qt Quick falls back to the software backend where ShaderEffect
    // never compiles (status stays Uncompiled) and effects silently don't render.
    // Capture on a real display (DISPLAY/Wayland session) instead.
    if (const QByteArray shot = qgetenv("CRT_KATE_SHOT"); !shot.isEmpty()) {
        QTimer::singleShot(1500, &app, [&engine, shot, &app]() {
            if (auto* root = engine.rootObjects().value(0)) {
                // Grab the actual WINDOW pixels. grabToImage() on the CRT
                // chain item can come back black depending on the scene-graph
                // pass ordering; grabWindow() shows what the user really sees
                // (including composited popups).
                if (auto* win = qobject_cast<QQuickWindow*>(root)) {
                    win->grabWindow().save(QString::fromUtf8(shot));
                    QCoreApplication::exit(0);
                    return;
                }
                QQuickItem *chain = root->findChild<QQuickItem*>(
                    QStringLiteral("crtEffectChain"));
                if (chain) {
                    auto result = chain->grabToImage();
                    if (result) {
                        QObject::connect(result.data(), &QQuickItemGrabResult::ready, &app,
                                         [result, shot, &app]() {
                            result->saveToFile(QString::fromUtf8(shot));
                            QCoreApplication::exit(0);
                        });
                        return;
                    }
                }
            }
            QCoreApplication::exit(0);
        });
    }

    return app.exec();
}