// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QObject>
#include <QUrl>
#include <QList>
#include <QPoint>
#include <QHash>
#include <QKeySequence>
#include <QStackedWidget>
// HistoryEntry holds a Cursor by value, so the definition is needed here.
#include <ktexteditor/cursor.h>

namespace KTextEditor { class Document; class View; }

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

    void newDocument();
    void openFile(const QUrl& url);
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

    // Word wrap (2026-11-19): Kate's own per-view config key
    // "dynamic-word-wrap" (bool). applyTvStyle() turns it on for every new
    // view; the View menu toggles the ACTIVE view only.
    bool wordWrap() const;
    void setWordWrap(bool on);

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
    void addToLines(bool atEnd, QWidget* anchor);
    void removeLineBreaks();

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
    void tabsChanged(const QStringList& titles, int active);
    // The ACTIVE view's cursor in KTextEditor's 0-based coordinates: emitted
    // when it moves, and re-emitted when the active tab changes so a listener
    // can show the new tab's position at once. The bottom band's "L:C" reads
    // this; binding a listener to the FIRST view instead left the indicator
    // frozen at 1:1 as soon as a session restored more than one file.
    void activeCursorPositionChanged(int line, int column);
    // Emitted whenever the active view changes (new tab, switch, close). The
    // status-line indicators re-point at the new view/document on this, so
    // none of them freeze on the first view (the L:C band bug class).
    void activeViewChanged(KTextEditor::View* view);
    // Emitted whenever the active document's history changes, so the Edit menu
    // can enable/disable Undo and Redo.
    void historyChanged(bool canUndo, bool canRedo);

private:
    KTextEditor::Document* createDoc();
    void syncTabs();
    // Points the activeCursorPositionChanged signal at the CURRENT view: drops
    // the previous view's connection, re-emits the new view's position, and
    // follows it from then on.
    void trackActiveCursor();
    // Deliver a FocusIn to the view's KateViewInternal canvas so Kate starts
    // its caret-blink timer (never fires on its own on this unmapped shell).
    void startCaretBlink(KTextEditor::View* view);
    // Route Ctrl+Z / Ctrl+Y from a focused view to undo()/redo(). The QML Keys
    // handler never sees the chord once the embedded QWidget owns focus, and
    // this KTextEditor build drops it, so the view needs its own path in.
    void installKeyRouting(KTextEditor::View* view);

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
    QList<KTextEditor::Document*> m_docs;
    QList<KTextEditor::View*> m_views;
    int m_active = -1;
    QMetaObject::Connection m_cursorConn;
    int m_basePointSize = -1;   // font point size at first view (zoom baseline)
    int m_basePixelSize = -1;   // font pixel size at first zoom (pixel-based font)
    QStackedWidget* m_container = nullptr;
    QString m_lastFind;   // last Search-menu pattern (for Find Next)

    QHash<KTextEditor::Document*, QList<HistoryEntry>> m_history;
    QHash<KTextEditor::Document*, int> m_historyIndex;
    // Guards against the history handler re-entering while undo/redo is
    // restoring text (Document::setText emits textChanged).
    bool m_applyingHistory = false;
    static constexpr int kMaxHistory = 200;
};