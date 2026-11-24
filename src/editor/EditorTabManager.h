// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QObject>
#include <QUrl>
#include <QList>
#include <QPoint>
#include <QHash>
#include <QKeySequence>
#include <QStackedWidget>
#include <QPointer>
#include <QFileSystemWatcher>
#include <functional>
class QIcon;
// HistoryEntry holds a Cursor by value, so the definition is needed here.
#include <ktexteditor/cursor.h>
#include "FileChangeTracker.h"
#include "BlockFinder.h"
#include "src/shell/HexColorScan.h"

class HexSwatchProvider;

namespace KTextEditor { class Document; class View; class MovingRange; }

// Owns the KTextEditor documents/views behind the Borland TabStrip.
// One KTextEditor::Document per tab; the active document's view is the editor.
// Provides zoom (Ctrl+=/-/0) and text tools (change case, sort lines).
class EditorTabManager : public QObject
{
    Q_OBJECT
public:
    enum CaseKind { Upper, Lower, Title };
    Q_ENUM(CaseKind)

    explicit EditorTabManager(QObject* parent = nullptr);
    ~EditorTabManager() override;

    KTextEditor::View* activeView() const;
    KTextEditor::Document* activeDocument() const;
    int activeIndex() const { return m_active; }
    int tabCount() const { return m_docs.size(); }
    // Container hosting the per-tab KTextEditor::Views (replaces ShellWindow's
    // central placeholder; main.cpp installs it via ShellWindow::setCentralWidget).
    QStackedWidget* containerWidget() const { return m_container; }

    // --- Split view (2026-11-21) ---
    // A pane is an independent KTextEditor::View: own caret, scroll, folds.
    // Pane 0 is the primary view per document (m_views); pane 1 is a lazily
    // created SECONDARY view (m_secondaryViews) shown only while split. The
    // active pane's view is what activeView() returns, so every existing
    // feature follows focus through activeViewChanged alone.
    QStackedWidget* secondaryContainerWidget();   // created on first split
    KTextEditor::View* viewForPane(int pane) const;
    // The colour-swatch inline-note provider attached to `view`, or nullptr.
    // Exposed so the shell can connect its swatchClicked signal and so tests
    // can prove every view actually got one.
    HexSwatchProvider* swatchProvider(KTextEditor::View* view) const;
    KTextEditor::Document* documentForPane(int pane) const;  // pane 1 null unless split
    bool splitOpen() const;
    void openSplitWithActiveDocument();          // View > Open Document in New View
    void openSplitWithDocument(KTextEditor::Document* doc);  // File > Open as Split
    void closeSplit();
    int activePane() const { return m_activePane; }
    void setActivePane(int pane);                // emits activeViewChanged

    void newDocument();
    void openFile(const QUrl& url);
    // File > Open as Split: add `url` as a tab WITHOUT activating it in pane 0,
    // then show it in the right pane. Plain openFile() activates the new tab,
    // so the open-then-split order used to mirror one file into both panes.
    void openFileAsSplit(const QUrl& url);
    void setActiveIndex(int index);
    // Close one tab (the chip's green square). The active index follows the
    // removal; closing the LAST tab leaves one fresh untitled document, so the
    // strip and the editor never go empty.
    void closeTab(int index);

    // Session snapshot accessors (Task 7): per-tab file URL + cursor position
    // and the active tab, so the composition root can persist a named session.
    int viewCount() const { return m_views.size(); }
    QUrl viewFileUrl(int index) const;
    QPoint viewCursorPosition(int index) const;

    // Every open document, in tab order. Find and Replace's "All documents"
    // scope walks these; the list is not copied for a single-document search.
    QList<KTextEditor::Document*> documents() const { return m_docs; }

    // Zoom: +1 in / -1 out / 0 reset, applied to the active view's "font"
    // config key (KTextEditor::View::configValue/setConfigValue). Zooms in
    // pixel units when the font is pixel-based (setFontSize), points otherwise.
    void setZoom(int delta);
    // Task 12: apply an absolute editor font size (px) to every view (the
    // Turbo Vision default 16px, user-changeable via the settings dialog).
    void setFontSize(int pixelSize);

    // Task 12: Turbo Vision editor styling (window-blue field, yellow text)
    // applied to a freshly created KTextEditor::View.
    void applyTvStyle(KTextEditor::View* view);
    // TV-revision Task 1: select the bundled Turbo Vision KSyntaxHighlighting
    // theme (:/crttv) so editor text is yellow on blue with no black current line.
    void applyTvTheme(KTextEditor::View* view);

    // Text tools on the active view (selection if present, else whole doc).
    void changeCase(CaseKind kind);
    void sortLines();

    // Edit menu (2026-11-20): Select All covers the WHOLE active document and
    // ignores any existing selection (unlike the text tools above, which prefer
    // one). Deselect clears the selection and leaves the caret where it is -
    // KTextEditor::View::removeSelection() does not move the cursor.
    void selectAllActive();
    void deselectActive();

    // Word wrap (2026-11-19): Kate's own per-view config key
    // "dynamic-word-wrap" (bool). applyTvStyle() turns it on for every new
    // view; the View menu toggles the ACTIVE view only.
    bool wordWrap() const;
    void setWordWrap(bool on);

    // View > Show / Hide Nonprintable Characters (2026-11-20): one command
    // drives Kate's own per-view actions view_show_whitespaces and
    // view_non_printable_spaces together, so the drawing stays Kate's. The
    // state is per view; the menu re-syncs on activeViewChanged. QAction
    // trigger() TOGGLES in this build (measured), so the setter triggers until
    // the state matches instead of calling setChecked first.
    bool nonprintablesVisible() const;
    void setNonprintables(bool on);

    // View > Coloured Brackets (2026-11-20): colour each bracket pair so nested
    // blocks are readable (Kate's addon ported - see the design doc). State is
    // per active view; trackActiveCursor() re-points the wiring on every tab
    // change, and the menu re-syncs its check on activeViewChanged.
    bool colouredBrackets() const;
    void setColouredBrackets(bool on);

    // View > Collapse All / Expand All / Toggle Current Node (2026-11-20): the
    // folding ("nesting") commands. Kate exposes these as per-view ACTIONS and
    // nothing else - there is no folding class in the 6.30 headers - so these
    // drive folding_toplevel / folding_expandtoplevel / folding_toggle_current.
    // Measured: the actions really fold (10 visible lines -> 4 -> 10) and the
    // state is per VIEW, so "per tab" is automatic.
    void collapseAll();
    void expandAll();
    void toggleCurrentNode();
    // A document with no syntax mode has no folding regions at all (measured:
    // the actions do nothing). There is no API to ask for fold regions, so the
    // mode is the honest proxy - the menu greys the commands out when false.
    bool canFold() const;

    // Block commands for the right-click menu (2026-11-20). "On a block" means
    // the caret is on the line that OPENS it - Kate's folding-marker rule - and
    // the extent comes from BlockFinder because Kate has no public fold-region
    // API. Kate offers only folding_toggle_current, so collapse/expand check the
    // folded state first (via View::isLineVisible) to stay idempotent.
    BlockFinder::Block currentBlock() const;
    bool canActOnBlock() const;
    bool currentBlockIsFolded() const;
    void collapseBlock();
    void expandBlock();
    // Clicking the gutter fold marker (the ▶/▼ in the icon strip) must fold or
    // unfold that LINE's block, not whatever line the caret happens to be on.
    // These move the caret onto `line` first, then run the same collapse/expand
    // the block menu uses. Installed by the FoldClickRouter event filter on
    // KateIconBorder (see EditorTabManager.cpp).
    void collapseBlockAtLine(int line);
    void expandBlockAtLine(int line);

    // The red-on-yellow band over the current block. Off by default; the user
    // toggles it from the right-click menu. Cleared when the caret leaves the
    // block or the tab changes, so it never highlights a stale region.
    void setBlockHighlight(bool on);
    bool blockHighlight() const { return m_blockHighlight; }
    // Clicking a gutter fold marker highlights that block (a green band with
    // yellow text) so the user can see which block they just folded or
    // unfolded. Clicking back into the text area clears it. This is transient
    // and independent of the persistent Highlight Block menu toggle.
    void highlightClickedBlock(int line);
    void clearClickedBlockHighlight();

    // One of actionCtxCollapseBlock / actionCtxExpandBlock /
    // actionCtxHighlightBlock, created with the views and kept in sync with the
    // caret. Null when the name is unknown.
    QAction* contextBlockAction(const QString& name) const;

    // Re-evaluate the three block commands against the caret. ShellWindow calls
    // this immediately before the right-click menu pops, because the menu acts
    // on the caret as it stood when the button went down.
    void refreshBlockMenuState();

    // Block markers (2026-11-20): the View menu's one switch for all block
    // decoration. Hidden by default, and applied at view creation rather than
    // only when the toggle fires, so a newly opened file comes up clean too.
    //
    // The lever is Kate's own per-view action `view_folding_markers`, NOT
    // View::setConfigValue: measured twice, a config write combined with a fold
    // segfaults this app, while triggering the action does not.
    void setBlockMarkersVisible(bool visible);
    bool blockMarkersVisible() const;

    // Gutter fold marker (2026-11-20): recompute + apply the `▶`/`▼` marks on
    // the active document, or clear them when the Show Block Markers toggle is
    // off / the document has no syntax mode. Called from setBlockMarkersVisible,
    // on textChanged, and on tab switch so the marks always track the toggle,
    // the content and the active tab. See FoldIndicator.h for which lines get
    // marked.
    void refreshFoldIndicator();
    // Swap the shared ▶/▼ glyph after a fold/unfold WITHOUT re-scanning the
    // document, clearing marks or writing the icon-bar config. The fold
    // commands run per click; a full refresh on each one churns Kate's layout
    // cache (setConfigValue -> updateConfig -> KateLayoutCache::clear) hard
    // enough to crash in ~QTextLayout (measured 2026-11-21). Only the shared
    // glyph changes on a fold - the marks stay on the same lines.
    void updateFoldGlyph();

    // Look up one of Kate's per-view actions by object name. Public because the
    // view's action collection is the only place these live (findChild on the
    // view returns null for them) and the menu wiring lives outside this class.
    QAction* viewActionByName(const QString& name) const;

    // Restore a saved set of files (2026-11-20). Opens each URL and then DROPS
    // the pristine untitled tab the constructor created, because openFile()
    // appends: without this the restored file lands in tab 1 while activeIndex 0
    // selects the empty placeholder, which is exactly how launch-time restore
    // used to come up blank. A tab the user has typed into is NOT a placeholder
    // and is never closed. `activeIndex` is clamped.
    void replaceWithFiles(const QVector<QUrl>& urls, int activeIndex);

    // Pure line transforms behind the Edit menu's line tools (2026-11-19).
    // affixLines() inserts `affix` at the start or end of every line; a CRLF's
    // \r stays with the terminator, and the empty segment after a trailing \n
    // is not a line. removeLineBreaksText() turns each run of breaks into one
    // space and leaves no stray space where the text began or ended with one.
    static QString affixLines(const QString& text, const QString& affix, bool atEnd);
    static QString removeLineBreaksText(const QString& text);
    // stripEdgeText() is affixLines' remove mode: it takes ONE leading (Start)
    // or trailing (End) occurrence of `needle` off every line. Only the edge is
    // stripped, once, so a match in the middle of a line survives.
    static QString stripEdgeText(const QString& text, const QString& needle, bool atEnd);

    // The Edit menu's line tools, applied to the active view (selection if
    // present, else whole doc). addToLines() collects the affix in the house
    // modal; affixActiveLines() is the apply step and is dialog-free so it can
    // be tested. An empty affix is a no-op: no edit, no undo entry.
    void affixActiveLines(const QString& affix, bool atEnd);
    // Remove mode of the line tools: strips `needle` off each line's edge
    // (selection if present, else whole doc), mirroring affixActiveLines. An
    // empty needle is a no-op: no edit, no undo entry.
    void stripActiveLines(const QString& needle, bool atEnd);
    void addToLines(bool atEnd, QWidget* anchor);
    void removeLineBreaks();

    // View > Go: move the caret to a 1-based line number (clamped to the
    // document). The dialog does the validation; this does the jump.
    void goToLine(int oneBasedLine);

    // Standard clipboard editing (Edit menu + Ctrl+X/C/V). This KTextEditor
    // build ships no ClipboardInterface and no action hooks, so these are
    // implemented over Document::text/removeText/insertText and QClipboard.
    void cut();
    void copy();
    void paste();

    // Undo / redo. This KTextEditor build ships no undo API (no UndoInterface,
    // no action hooks - verified against the installed headers), so the history
    // is kept here: one text+cursor snapshot per document, pushed on every
    // Document::textChanged.
    void undo();
    void redo();
    bool canUndo() const;
    bool canRedo() const;

    // File ops for the File menu (Task 10): documentSave/documentSaveAs on the
    // active document. NOTE: both can open a native file dialog (save-as, or
    // save of an untitled buffer) - never call them from automated tests.
    bool saveActive();
    bool saveActiveAs();
    // Save the active document to an explicit URL (the TV file dialog already
    // knows the path, so it must not open a native prompt of its own).
    bool saveActiveAs(const QUrl& url);

    // Search menu: findReplace() opens the house-style Find and Replace modal
    // over `anchor` with the Find what (0) or Replace with (1) field active.
    // findNext() repeats the last search with the dialog closed (F3 / the menu
    // item), through the same dialog class so there is ONE matching path.
    void findReplace(QWidget* anchor, int whichField);
    void findNext();

    // External-change prompt (2026-11-20). Kate's own modifiedOnDisk never
    // fires in this build (its library carries no file watcher), so this class
    // owns one: every open local file is watched, and a real change or deletion
    // opens a ReloadDialog over the anchor the composition root supplied.
    // setExternalChangeHandler is the test seam: it answers with a TvModal role
    // instead of opening the modal.
    using ExternalChangeHandler = std::function<int(FileChange, const QString&)>;
    void setDialogAnchor(QWidget* anchor) { m_dialogAnchor = anchor; }
    void setExternalChangeHandler(ExternalChangeHandler handler)
    {
        m_externalHandler = std::move(handler);
    }
    // Reload from disk WITHOUT leaving an undo step that would restore the
    // buffer the user chose to discard: the custom history is re-seeded to the
    // reloaded text.
    void reloadDocument(KTextEditor::Document* doc);
    // Reflects the REAL watcher, not just our bookkeeping: Qt silently drops a
    // path that disappears, so a Keep-then-unsaved buffer is not "watching".
    bool isWatchingPath(const QString& path) const
    {
        return !path.isEmpty() && m_watcher.files().contains(path);
    }

    // Installs zoom shortcuts (Ctrl+= / Ctrl+- / Ctrl+0) and text-tool actions
    // (change case, sort) as children of \p host (typically the ShellWindow).
    void installActions(QWidget* host);
    // Fires the ACTIVE view's QAction whose shortcut matches \p seq, if it is
    // enabled. KTextEditor keeps its editing actions (cursor movement,
    // Backspace, Delete, PageUp/PageDown) in a KActionCollection that is NOT
    // parented into the widget tree, so Qt's shortcut map is the only thing
    // that could reach them - and it never runs here (the shell is never the
    // active window). Returns true when an action fired.
    bool triggerActiveViewShortcut(const QKeySequence& seq);

signals:
    // A colour swatch was clicked: the token it belongs to, its line, and the
    // box's viewport rect so the shell can hang the picker beside it.
    void swatchClicked(const HexColorToken& token, int line, const QRect& cellRect);
    void tabsChanged(const QStringList& titles, int active);
    // Split opened/closed (2026-11-21): the shell shows/hides pane 1 and its
    // divider in response, and the composition root re-points per-pane chrome.
    void splitChanged(bool open);
    // The ACTIVE view's cursor in KTextEditor's 0-based coordinates: emitted
    // when it moves, and re-emitted when the active tab changes so a listener
    // can show the new tab's position at once. The bottom band's "L:C" reads
    // this; binding a listener to the FIRST view instead left the indicator
    // frozen at 1:1 as soon as a session restored more than one file.
    void activeCursorPositionChanged(int line, int column);
    // Per-PANE cursor (2026-11-23): emitted for the pane whose view's caret
    // moved (0 or 1), and re-emitted for both panes whenever the split or the
    // pane-0 tab changes. Each pane's bottom band shows ITS OWN "L:C" - feeding
    // the single active view left the created pane's band frozen at 1:1 and
    // made pane 0's band echo pane 1's caret.
    void paneCursorPositionChanged(int pane, int line, int column);
    // Emitted whenever the active view changes (new tab, switch, close). The
    // status-line indicators re-point at the new view/document on this, so
    // none of them freeze on the first view (the L:C band bug class).
    void activeViewChanged(KTextEditor::View* view);
    // Emitted whenever the active document's history changes, so the Edit menu
    // can enable/disable Undo and Redo.
    void historyChanged(bool canUndo, bool canRedo);
    // Emitted before the prompt is answered, for observers and tests.
    void externalChangeDetected(FileChange kind, const QString& path);

private:
    KTextEditor::Document* createDoc();
    // Shared tab creation: builds the document + primary view and registers the
    // tab. `activate` moves pane 0 onto the new tab (plain File > Open); false
    // leaves pane 0 exactly where it was (File > Open as Split).
    KTextEditor::Document* addTabForUrl(const QUrl& url, bool activate);
    void syncTabs();
    // Points the activeCursorPositionChanged signal at the CURRENT view: drops
    // the previous view's connection, re-emits the new view's position, and
    // follows it from then on.
    void trackActiveCursor();
    // Binds a caret listener per pane (0 and 1) and re-emits each pane's
    // current position, so both bands are seeded on every tab/split change.
    void trackPaneCursors();
    // Registers a swatch provider on a freshly created view (2026-11-23). Every
    // creation path calls this, so no view can exist without its swatches.
    void attachSwatchProvider(KTextEditor::View* view);
    // Deliver a FocusIn to the view's KateViewInternal canvas so Kate starts
    // its caret-blink timer (never fires on its own on this unmapped shell).
    void startCaretBlink(KTextEditor::View* view);
    // Route Ctrl+Z / Ctrl+Y from a focused view to undo()/redo(). The QML Keys
    // handler never sees the chord once the embedded QWidget owns focus, and
    // this KTextEditor build drops it, so the view needs its own path in.
    void installKeyRouting(KTextEditor::View* view);
    // Watch a document's local file: baseline its signature, add the path (and
    // its directory, so an atomic replace is still seen) to the watcher. Also
    // the own-save re-baseline, so our own write never prompts.
    void armWatch(KTextEditor::Document* doc);
    void disarmWatch(KTextEditor::Document* doc);
    void onWatchedPathChanged(const QString& path);

    // One undo step: the whole document text plus where the caret sat.
    struct HistoryEntry {
        QString text;
        KTextEditor::Cursor cursor;
    };
    void watchHistory(KTextEditor::Document* doc);
    void pushHistory(KTextEditor::Document* doc);
    QList<HistoryEntry>& historyFor(KTextEditor::Document* doc);
    int& historyIndexFor(KTextEditor::Document* doc);
    void applyHistoryStep(KTextEditor::Document* doc, int index);
    void notifyHistory();
    void applyZoom(int delta);
    // Per-view setup shared by primary and secondary (pane 1) views.
    void finishView(KTextEditor::View* view);
    KTextEditor::View* ensureSecondaryView(KTextEditor::Document* doc);
    void destroySecondaryViews();   // synchronous teardown (see dtor)
    bool anyLiveSecondary() const;  // true if any pane-1 view is still ALIVE
                                    // (m_secondaryViews holds QPointers, which
                                    // null when the shell's pane teardown kills them)
    QList<KTextEditor::Document*> m_docs;
    QList<KTextEditor::View*> m_views;              // pane 0 (primary) view per doc
    // One swatch provider per view, owned here (parented to the manager) and
    // dropped with the view.
    QHash<KTextEditor::View*, HexSwatchProvider*> m_swatchProviders;
    QHash<KTextEditor::Document*, QPointer<KTextEditor::View>> m_secondaryViews;  // pane 1; QPointer so a view destroyed by the shell's pane teardown self-nulls
    int m_active = -1;
    int m_activePane = 0;                            // which pane owns focus
    QPointer<QStackedWidget> m_stack1;         // pane 1's view stack (lazy); QPointer
                                               // self-clears if the shell destroys it
                                               // (clearSplitEditorWidget -> pane1 death)
    KTextEditor::Document* m_splitDoc = nullptr;     // the document shown in pane 1
    QMetaObject::Connection m_cursorConn;
    QMetaObject::Connection m_paneCursorConns[2];   // per-pane caret feeds
    int m_basePointSize = -1;   // font point size at first view (zoom baseline)
    int m_basePixelSize = -1;   // font pixel size at first zoom (pixel-based font)
    QStackedWidget* m_container = nullptr;
    QString m_lastFind;   // last Search-menu pattern (for Find Next)

    QHash<KTextEditor::Document*, QList<HistoryEntry>> m_history;
    QHash<KTextEditor::Document*, int> m_historyIndex;
    QFileSystemWatcher m_watcher;
    FileChangeTracker m_tracker;
    ExternalChangeHandler m_externalHandler;
    QWidget* m_dialogAnchor = nullptr;
    bool m_externalPromptOpen = false;
    // path -> document, so a watcher event can find its buffer after a tab move.
    QHash<QString, KTextEditor::Document*> m_watchedDoc;
    // Guards against the history handler re-entering while undo/redo is
    // restoring text (Document::setText emits textChanged).
    bool m_applyingHistory = false;
    // Colouring of bracket pairs for the ACTIVE view (owns its MovingRanges).
    class ColourWiring* m_brackets = nullptr;
    bool m_bracketsOn = true;
    // Block commands (2026-11-20): the three context-menu actions, the highlight
    // toggle, and the MovingRange that paints the band (owned by the document).
    QAction* m_ctxCollapseBlock = nullptr;
    QAction* m_ctxExpandBlock = nullptr;
    QAction* m_ctxHighlightBlock = nullptr;
    bool m_blockHighlight = false;
    KTextEditor::MovingRange* m_blockHighlightRange = nullptr;
    void buildBlockActions();
    void refreshBlockActions();
    void applyBlockHighlight();
    void clearBlockHighlightRange();
    void applyBlockRange(const BlockFinder::Block& b);
    void attachBlockTracking(KTextEditor::View* view);
    // Gutter fold marker: clear existing Bookmark marks then add one per
    // foldable line, and set the `▶`/`▼` icon. No-op (clears) without a mode.
    void applyFoldMarks();
    // The yellow `▼` (expanded) or `▶` (folded) gutter icon, built once.
    QIcon foldIcon(bool folded) const;
    // The shared "text the highlighter owns" skip probe, so currentBlock() and
    // applyFoldMarks() agree on which positions can open a block.
    static BlockFinder::SkipProbe blockSkipProbe(KTextEditor::Document* doc);
    static constexpr int kMaxHistory = 200;
};