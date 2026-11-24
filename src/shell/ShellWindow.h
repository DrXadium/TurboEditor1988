// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QMainWindow>
#include <QKeySequence>
#include <QString>
#include <functional>
#include <QVBoxLayout>
#include <QPointer>
#include <QScrollBar>
class MenuBar;
class TabStrip;
class FKeyStatusBar;
class QAction;
class BorlandStyle;
class BorlandMenu;
class ContentBridge;
namespace Sonnet { class Speller; }

// The word the user right-clicked on, resolved for the spell section.
// line/column are KTextEditor coordinates (line, column).
struct SpellWord {
    QString text;
    int line = -1;
    int startCol = -1;
    int endCol = -1;      // exclusive
    bool valid() const { return line >= 0 && startCol >= 0 && endCol > startCol; }
};

// Top-level Borland chrome window: MenuBar (top), TabStrip, central editor
// placeholder, FKeyStatusBar (bottom). Uses BorlandStyle. CRT effects are
// applied over this whole window in Task 4 (via ContentBridge).
class ShellWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit ShellWindow(QWidget* parent = nullptr);
    // Central editor host (replaced by KTextEditor in Task 5).
    void setCentralWidget(QWidget* w);
    // Replaces the placeholder area BELOW the TabStrip with the real editor
    // widget (EditorTabManager::container). TabStrip stays intact above it.
    void setEditorWidget(QWidget* w);
    // Name shown in the document's TOP border row (the ╔ ═ name ═ ╗ row drawn
    // below the tab row). The composition root calls this on every tab change.
    void setEditorTitle(const QString& name);
    // Re-binds the painted right/bottom scrollbars to the document scrollbars
    // of the current view (pass KTextEditor::View::verticalScrollBar() and
    // horizontalScrollBar(); crt-shell does not link KTextEditor, so the
    // caller does that lookup). Safe to call again: it disconnects the
    // previous pair first, so it reconnects cleanly when the active tab
    // (and therefore the view) changes.
    void wireScrollbars(QScrollBar* docV, QScrollBar* docH, QWidget* view = nullptr);
    // Per-pane variant (split view): bind pane `pane`'s own right bar + bottom
    // band to a view's scrollbars. Independent per pane, so pane 1's chrome
    // follows pane 1's view without touching pane 0's. No-op for pane 1 when
    // no split is open.
    void wirePaneScrollbars(int pane, QScrollBar* docV, QScrollBar* docH);
    // Editor right-click menu (edit commands +, when the word under the click
    // is misspelled, the spell section). Built as a BorlandMenu so it paints
    // like the dropdowns, and shown at a shell-local point. The shell is never
    // mapped, so this must NOT go through QMenu::exec(): the menu is popup()'d
    // and driven by synthesized events from ContentBridge, like the menu bar.
    //
    // \p word is optional: the bridge passes the word under the pointer so the
    // spell section can be built. Passing a correct word (or nothing) leaves
    // the menu exactly as Task 2 left it.
    void showEditorContextMenu(const QPoint& shellLocal);
    void showEditorContextMenu(const QPoint& shellLocal, const SpellWord& word);
    // The popout for the misspelled word (suggestions / add / ignore), or
    // nullptr when the section was not built. Exposed for tests: no pointer
    // exists on the capture host, so the popout is reached through this.
    BorlandMenu* spellPopout() const { return m_spellPopout; }
    bool spellPopoutVisible() const;
    // The bridge that owns popup registration, so the popout gets composited
    // and receives synthesized hover like every other menu. NOT a QPointer:
    // crt-shell only forward-declares ContentBridge (QPointer needs the full
    // type, which would drag QQuickImageProvider into this header). The bridge
    // is owned by the composition root and outlives the shell's spell state.
    void setSpellPopupHost(ContentBridge* bridge);
    // Show the popout hanging off the right of the context menu's headline
    // row, so hovering it is what the bridge sees next.
    void showSpellPopout();
    // What the spell section decided, as data (crt-shell never links
    // KTextEditor, so the composition root performs the edit).
    const SpellWord& spellWord() const { return m_spellWord; }
    // Resolver the BRIDGE uses to find the word under a right-click. Like
    // setEditorShortcutHandler, it is installed by the composition root (which
    // owns the KTextEditor document); crt-shell only calls it. Given a
    // shell-local point it returns the word and its range, or an invalid
    // SpellWord when the click is not on a word.
    void setSpellWordResolver(std::function<SpellWord(const QPoint&)> fn);

    // Block commands for the right-click menu (2026-11-20). Kate's own
    // contextMenuAboutToShow never fires here: the shell is never mapped and
    // ContentBridge synthesizes the press, so the menu that actually appears is
    // this shell's BorlandMenu. The actions stay owned by EditorTabManager;
    // refresh re-evaluates their enabled state against the caret before the
    // menu pops, and must not delete them.
    void setBlockMenuActions(QAction* collapse, QAction* expand, QAction* highlight,
                             std::function<void()> refresh);
    // Ask the resolver for the word at \p shellLocal (invalid when unset).
    SpellWord spellWordAt(const QPoint& shellLocal) const;
    // Dictionary the spell section speaks. The composition root sets it from
    // the speller the status selector owns, so the popout's suggestions and the
    // status-bar language tag are ALWAYS the same dictionary - a word could not
    // be "misspelled" in one place and correct in another. Unset = Sonnet's own
    // default.
    void setSpellLanguage(const QString& lang);
    QString spellLanguage() const { return m_spellLang; }
    // True when \p w is the editor widget or inside it (used by the bridge to
    // decide that a right-click belongs to the editor's context menu).
    bool isEditorArea(QWidget* w) const;
    // Focus target for forwarded keys: the editor if one is set, else the shell.
    QWidget* keyFocusWidget() const;
    // Keyboard ownership. While a visible owner is set (the file dock, a house
    // modal), ContentBridge delivers the key to the focused widget and stops:
    // no menu shortcut, no editor shortcut. Without this, Ctrl+S saved the
    // document, Ctrl+X cut it and Ctrl+F opened Find BEHIND the open dialog.
    // Cleared by the owner itself when it hides.
    void setKeyboardOwner(QWidget* owner);
    QWidget* keyboardOwner() const { return m_keyboardOwner; }
    bool keyboardIsOwned() const;
    // Editor shortcut fallback. The bridge cannot reach KTextEditor's editing
    // actions: crt-shell does not link KTextEditor, and Kate keeps those
    // actions in a KActionCollection that is NOT parented into the widget tree.
    // The composition root installs the handler that fires the matching one.
    void setEditorShortcutHandler(std::function<bool(const QKeySequence&)> fn);
    bool triggerEditorShortcut(const QKeySequence& seq) const;
    TabStrip* tabStrip() const;
    FKeyStatusBar* fkeyStatusBar() const;
    // Bottom border band (cursor position + horizontal scrollbar), spec 5h/6.
    class BottomBand* bottomBand() const;
    // The code minimap shown beside the editor (Task 9), or nullptr before
    // setEditorWidget() installs the editor row.
    class Minimap* minimap() const { return m_minimap; }
    // Split view (2026-11-21): whether a right pane is open, which pane widget
    // sits at index i (0 exists once setEditorWidget ran; 1 is nullptr until a
    // split), and which pane a click landed in (the active-pane signal source,
    // Task 6). All defined in the .cpp - crt-shell only forward-declares
    // EditorPane/EditorSplitter here.
    bool splitOpen() const;
    class EditorPane* paneWidget(int i) const;
    void notePressInWidget(QWidget* w);
    // Which pane currently owns focus (0 or 1). A press inside a pane sets it;
    // a tab click does not (Review Focus 3).
    int activePane() const { return m_activePane; }
    // Install the secondary stack into pane 1 and open the split: pane 1 shows
    // `name` in its title row and gets the ■ close box; pane 0 keeps its title.
    // `stack` is the per-pane view stack (EditorTabManager::secondaryContainer).
    void setSplitEditorWidget(QWidget* stack, const QString& name);
    // Collapse the split: hide pane 1 + divider, hide its ■ (pane 0 stays).
    void clearSplitEditorWidget();
    // The Borland style this window installed (QSS wrapping hides it from
    // w.style(), so the test asserts on this directly).
    BorlandStyle* borlandStyle() const { return m_borland; }
protected:
    void paintEvent(QPaintEvent*) override;
signals:
    // The pane under the last press became active (0 or 1). The composition
    // root forwards this to EditorTabManager::setActivePane, so activeView()
    // re-points the minimap/status/fold to the focused pane.
    void activePaneChanged(int pane);
    // The split was torn down through the pane-1 ■ box (or any other SHELL
    // path). The composition root forwards this to EditorTabManager::closeSplit()
    // so the manager's split state cannot outlive pane 1 - a stale
    // m_activePane == 1 made activeView() point at the destroyed secondary view,
    // which blanked every statistic on the status line.
    void splitCleared();
    // Picked a suggestion for the right-clicked word: replace [line, startCol)
    // .. endCol) with \p replacement.
    void spellReplacementRequested(int line, int startCol, int endCol,
                                   const QString& replacement);
    // "Ignore word": suppress \p word starting at (line, startCol) in this
    // document only.
    void spellIgnoreRequested(int line, int startCol, const QString& word);
private:
    // Helpers behind showEditorContextMenu's spell section (GUI pass, Task 3).
    void buildSpellSection(BorlandMenu* menu, const SpellWord& word,
                           const Sonnet::Speller& speller);
    void buildSpellPopout(const QStringList& suggestions,
                          const Sonnet::Speller& speller);
    void applySpellReplacement(const QString& replacement);
    void ignoreSpellWord();

    BorlandStyle* m_borland = nullptr;
    QVBoxLayout* m_editorLayout = nullptr;   // central layout: [TabStrip, editorRow]
    class EditorPane* m_pane0 = nullptr;     // pane 0: frame + title + chrome
    class EditorSplitter* m_splitter = nullptr;   // hosts pane0 | [pane1] | minimap
    int m_activePane = 0;                        // which pane owns focus (0/1)
    // Per-pane scrollbar bindings (split view): the docV/docH currently wired
    // to each pane's chrome, so a re-bind can disconnect cleanly.
    QScrollBar* m_paneDocV[2] = {nullptr, nullptr};
    QScrollBar* m_paneDocH[2] = {nullptr, nullptr};
    QWidget* m_editor = nullptr;             // the splitter (or editor) for isEditorArea
    QWidget* m_editorWidget = nullptr;       // the installed editor widget (focus target)
    QWidget* m_keyboardOwner = nullptr;      // dock/modal that owns the keyboard
    std::function<bool(const QKeySequence&)> m_editorShortcut;
    std::function<SpellWord(const QPoint&)> m_spellResolver;
    std::function<void()> m_blockRefresh;
    QAction* m_blockCollapse = nullptr;
    QAction* m_blockExpand = nullptr;
    QAction* m_blockHighlight = nullptr;
    QString m_spellLang;
    Minimap* m_minimap = nullptr;
    class BottomBand* m_bottomBand = nullptr;
    // TV-revision2 spec 1: 1-cell right-border scrollbar + the (hidden) Kate
    // document vertical it mirrors. QPointer: the view can die on tab close.
    class RightScrollBar* m_rightBar = nullptr;
    QPointer<QScrollBar> m_docV;
    // Spell section state (GUI pass, Task 3). The popout is created per right
    // click so its rows always describe the word actually under the pointer.
    BorlandMenu* m_spellPopout = nullptr;
    SpellWord m_spellWord;
    ContentBridge* m_spellBridge = nullptr;
    // The view's own horizontal scrollbar, mirrored into the bottom band. The
    // old code only ever bound the VERTICAL one (and bound it to the wrong
    // child). QPointer: a tab close destroys the view and its scrollbars.
    QPointer<QScrollBar> m_docH;
};
