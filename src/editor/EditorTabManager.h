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

    // Search menu (Task 10): find() prompts for a pattern (QInputDialog) and
    // jumps to the first match after the cursor; findNext() repeats the last
    // pattern. Core matching lives in jumpToNextMatch().
    void find();
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
    bool jumpToNextMatch(const QString& pattern);

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