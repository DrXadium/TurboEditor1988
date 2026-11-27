// SPDX-License-Identifier: GPL-3.0-or-later
#include "MenuBar.h"
#include "BorlandMenu.h"
#include "src/AppIdentity.h"
#include <QDir>
#include <QPalette>
#include <QColor>
#include <QKeySequence>
#include <QPainter>

namespace {
// Turbo Vision palette: window blue #0000AA desktop, gray #AAAAAA bars,
// yellow #FFFF55 text, red #AA0000 hotkey — mirrors borland.qss so the
// widget's own palette agrees with the stylesheet visuals (tests assert
// on the palette directly).
void applyBorlandPalette(QWidget* w)
{
    QPalette pal = w->palette();
    const QColor windowBlue(0x00, 0x00, 0xAA);
    const QColor gray(0xAA, 0xAA, 0xAA);
    const QColor black(0, 0, 0);
    const QColor yellow(0xFF, 0xFF, 0x55);
    const QColor red(0xAA, 0x00, 0x00);
    for (auto group : { QPalette::Active, QPalette::Inactive, QPalette::Disabled }) {
        pal.setColor(group, QPalette::Window, windowBlue);
        pal.setColor(group, QPalette::Button, gray);
        pal.setColor(group, QPalette::WindowText, black);
        pal.setColor(group, QPalette::ButtonText, black);
        pal.setColor(group, QPalette::Base, windowBlue);
        pal.setColor(group, QPalette::Text, yellow);
        pal.setColor(group, QPalette::Highlight, windowBlue);
        pal.setColor(group, QPalette::HighlightedText, red);
    }
    w->setPalette(pal);
}

QAction* added(QMenu* m, const QString& text, const QString& objectName)
{
    QAction* a = m->addAction(text);
    a->setObjectName(objectName);
    return a;
}
} // namespace

MenuBar::MenuBar(QWidget* parent) : QMenuBar(parent)
{
    setObjectName(QStringLiteral("menuBar"));
    applyBorlandPalette(this);

    // Kate's option menus (spec: File, Edit, Search, View, Session, Settings,
    // Help — Kate's vocabulary, Borland's visual style). Entries carry
    // objectNames; the composition root (main.cpp) wires them to
    // EditorTabManager / SessionManager / SettingsDialog.
    QMenu* file = addMenu(QStringLiteral("File"));
    added(file, QStringLiteral("&New"), QStringLiteral("actionFileNew"));
    added(file, QStringLiteral("&Open..."), QStringLiteral("actionFileOpen"));
    // Split view (2026-11-21): open a file into the RIGHT pane of a new split.
    added(file, QStringLiteral("&Open as Split Document..."),
          QStringLiteral("actionFileOpenAsSplit"));
    file->addSeparator();
    added(file, QStringLiteral("&Save"), QStringLiteral("actionFileSave"));
    added(file, QStringLiteral("Save &As..."), QStringLiteral("actionFileSaveAs"));
    // Recent files (user request 2026-11-24). Built empty and hidden; the
    // composition root feeds it from RecentFilesStore and the Settings switch.
    file->addSeparator();
    m_recentMenu = new BorlandMenu(file);
    m_recentMenu->setObjectName(QStringLiteral("menuFileRecentFiles"));
    m_recentEntry = file->addMenu(m_recentMenu);
    m_recentEntry->setObjectName(QStringLiteral("actionFileRecentFiles"));
    m_recentEntry->setText(QStringLiteral("Recent &Files"));
    m_recentEntry->setVisible(false);
    // Graceful exit (2026-11-26, user request): the File menu's last row.
    // QCoreApplication::quit() runs the aboutToQuit handlers, which is where
    // the session snapshot and the CRT settings are persisted.
    file->addSeparator();
    QAction* exitAct = added(file, QStringLiteral("E&xit"),
                             QStringLiteral("actionFileExit"));
    exitAct->setShortcut(QKeySequence(QKeySequence::Quit));

    QMenu* edit = addMenu(QStringLiteral("Edit"));
    // Undo / redo head the menu, as in Kate and Turbo Vision. Both start
    // DISABLED: a fresh buffer has no history. The composition root enables
    // them from EditorTabManager::historyChanged.
    QAction* undoAct = added(edit, QStringLiteral("&Undo\tCtrl+Z"),
                             QStringLiteral("actionEditUndo"));
    QAction* redoAct = added(edit, QStringLiteral("&Redo\tCtrl+Y"),
                             QStringLiteral("actionEditRedo"));
    undoAct->setShortcut(QKeySequence(QStringLiteral("Ctrl+Z")));
    redoAct->setShortcut(QKeySequence(QStringLiteral("Ctrl+Y")));
    undoAct->setEnabled(false);
    redoAct->setEnabled(false);
    edit->addSeparator();
    // Standard clipboard block, as in Kate (and Turbo Vision).
    added(edit, QStringLiteral("&Cut\tCtrl+X"), QStringLiteral("actionEditCut"));
    added(edit, QStringLiteral("C&opy\tCtrl+C"), QStringLiteral("actionEditCopy"));
    added(edit, QStringLiteral("&Paste\tCtrl+V"), QStringLiteral("actionEditPaste"));
    // 2026-11-20: whole-document selection. No shortcut text is shown: Kate's
    // Ctrl+A is not exposed as an action in this build, so the menu must not
    // promise a chord nobody verified.
    added(edit, QStringLiteral("Select &All"), QStringLiteral("actionEditSelectAll"));
    added(edit, QStringLiteral("&Deselect All"), QStringLiteral("actionEditDeselectAll"));
    edit->addSeparator();
    added(edit, QStringLiteral("&UPPERCASE"), QStringLiteral("actionEditUpper"));
    added(edit, QStringLiteral("&lowercase"), QStringLiteral("actionEditLower"));
    added(edit, QStringLiteral("&Title Case"), QStringLiteral("actionEditTitle"));
    // Line tools: their own group, flat like every other TV menu. Sort Lines
    // heads it; the three new items follow (2026-11-19).
    edit->addSeparator();
    added(edit, QStringLiteral("&Sort Lines"), QStringLiteral("actionEditSort"));
    added(edit, QStringLiteral("Modify &End of Line..."), QStringLiteral("actionEditAddEol"));
    added(edit, QStringLiteral("Modify &Start of Line..."), QStringLiteral("actionEditAddBol"));
    added(edit, QStringLiteral("&Remove Line Breaks"), QStringLiteral("actionEditRemoveBreaks"));
    edit->addSeparator();
    QAction* spell = added(edit, QStringLiteral("Check Spelling &On-the-fly"),
                           QStringLiteral("actionEditSpellcheck"));
    spell->setCheckable(true);

    QMenu* search = addMenu(QStringLiteral("Search"));
    QAction* findAct = added(search, QStringLiteral("&Find...\tCtrl+F"),
                             QStringLiteral("actionSearchFind"));
    findAct->setShortcut(QKeySequence(QStringLiteral("Ctrl+F")));
    QAction* replaceAct = added(search, QStringLiteral("Re&place...\tCtrl+H"),
                                QStringLiteral("actionSearchReplace"));
    replaceAct->setShortcut(QKeySequence(QStringLiteral("Ctrl+H")));
    added(search, QStringLiteral("Find &Next"), QStringLiteral("actionSearchFindNext"));

    QMenu* view = addMenu(QStringLiteral("View"));
    // Split view (2026-11-21): a second view of the ACTIVE document (own caret/
    // folds), so the user can look at two places at once. First row of View.
    QAction* newView = added(view, QStringLiteral("&Open Document in New View"),
                             QStringLiteral("actionViewOpenInNewView"));
    newView->setStatusTip(QStringLiteral("Show a second view of this document"));
    view->addSeparator();
    QAction* zoomIn = added(view, QStringLiteral("Zoom &In"),
                            QStringLiteral("actionViewZoomIn"));
    zoomIn->setStatusTip(QStringLiteral("Increase the text size"));
    QAction* zoomOut = added(view, QStringLiteral("Zoom &Out"),
                             QStringLiteral("actionViewZoomOut"));
    zoomOut->setStatusTip(QStringLiteral("Decrease the text size"));
    // Spec 6: only Reset Zoom carries a shortcut, shown right-aligned (F5).
    // A tab in the action text renders the shortcut without binding a key.
    QAction* zoomReset = added(view, QStringLiteral("&Reset Zoom\tF5"),
                               QStringLiteral("actionViewZoomReset"));
    zoomReset->setStatusTip(QStringLiteral("Restore the default text size"));
    view->addSeparator();
    QAction* minimap = added(view, QStringLiteral("Show &Minimap"),
                             QStringLiteral("actionViewMinimap"));
    minimap->setCheckable(true);
    minimap->setChecked(false);   // TV-revision: minimap off by default
    minimap->setStatusTip(QStringLiteral("Toggle the document minimap"));
    // Block markers (2026-11-20): one switch for all block decoration - the
    // gutter markers and the rule Kate draws on a block's opening line. HIDDEN
    // by default at the user's request. The object name stays actionViewFoldBar
    // so the composition root's lookup and the existing menu tests keep working.
    QAction* foldBar = added(view, QStringLiteral("Show &Block Markers"),
                             QStringLiteral("actionViewFoldBar"));
    foldBar->setCheckable(true);
    foldBar->setChecked(false);
    foldBar->setStatusTip(QStringLiteral("Toggle the block markers"));
    // 2026-11-20: the folding ("nesting") commands. Each drives Kate's own
    // per-view action; folding state is per VIEW, so they are per tab for free.
    // Collapse All is Kate's "Fold Toplevel Nodes" - outermost blocks only, as
    // there is no API to walk the folding tree. Expand All is the default state.
    // Greyed out (in the composition root) when the document cannot fold.
    QAction* collapse = added(view, QStringLiteral("&Collapse All"),
                              QStringLiteral("actionViewCollapseAll"));
    collapse->setStatusTip(QStringLiteral("Fold the outermost blocks"));
    QAction* expand = added(view, QStringLiteral("E&xpand All"),
                            QStringLiteral("actionViewExpandAll"));
    expand->setStatusTip(QStringLiteral("Unfold every block"));
    QAction* toggleNode = added(view, QStringLiteral("Toggle C&urrent Block"),
                                QStringLiteral("actionViewToggleNode"));
    toggleNode->setStatusTip(QStringLiteral("Fold or unfold the block at the cursor"));
    // 2026-11-19: wrap is per view and ON by default (applyTvStyle). The check
    // follows the active tab, re-synced in the composition root.
    QAction* wrap = added(view, QStringLiteral("&Word Wrap"),
                          QStringLiteral("actionViewWordWrap"));
    wrap->setCheckable(true);
    wrap->setChecked(true);
    wrap->setStatusTip(QStringLiteral("Wrap long lines to the window width"));
    // 2026-11-20: one item drives Kate's TWO per-view whitespace actions
    // ("Show Whitespace" + "Show Non-Printable Spaces"). The state is per view,
    // so the composition root re-syncs the check on every tab switch - the
    // Word Wrap pattern.
    QAction* nonprint = added(view, QStringLiteral("Show / Hide &Nonprintable Characters"),
                              QStringLiteral("actionViewNonprintable"));
    nonprint->setCheckable(true);
    nonprint->setChecked(false);
    nonprint->setStatusTip(QStringLiteral("Show spaces, tabs and non-printable characters"));
    // 2026-11-20: colour each bracket pair so nested blocks are readable.
    // Kate's addon (rainbow-parens) ported as a MovingRange colourer; on by
    // default, re-synced on tab switch like Word Wrap and Nonprintable.
    QAction* brackets = added(view, QStringLiteral("Coloured &Brackets"),
                              QStringLiteral("actionViewBrackets"));
    brackets->setCheckable(true);
    brackets->setChecked(true);
    brackets->setStatusTip(QStringLiteral("Colour nested bracket pairs so they are easy to match"));
    // 2026-11-19: a plain jump to a line number, last row of the View menu.
    QAction* go = added(view, QStringLiteral("&Go..."),
                        QStringLiteral("actionViewGo"));
    go->setStatusTip(QStringLiteral("Jump to a line number"));

    QMenu* session = addMenu(QStringLiteral("Se&ssion"));
    added(session, QStringLiteral("&Save Session..."), QStringLiteral("actionSessionSave"));
    added(session, QStringLiteral("Restore &Last Session"), QStringLiteral("actionSessionRestore"));

    QMenu* settings = addMenu(QStringLiteral("Se&ttings"));
    added(settings, QStringLiteral("&Configure ") + AppIdentity::displayName()
                    + QStringLiteral("..."),
          QStringLiteral("actionSettingsConfigure"));

    QMenu* help = addMenu(QStringLiteral("&Help"));
    added(help, QStringLiteral("&About ") + AppIdentity::displayName(),
          QStringLiteral("actionHelpAbout"));
}

void MenuBar::setRecentFiles(const QStringList& paths)
{
    // The dropdown owns the "last five" rule, so a caller holding a longer
    // MRU (RecentFilesStore keeps ten) cannot flood the menu.
    m_recentPaths = paths.mid(0, kRecentCount);
    rebuildRecentFiles();
}

void MenuBar::setRecentFilesVisible(bool on)
{
    m_recentsWanted = on;
    rebuildRecentFiles();
}

void MenuBar::rebuildRecentFiles()
{
    if (!m_recentMenu || !m_recentEntry)
        return;
    // Removing the old rows destroys the previous actions; the lambdas go with
    // them, so a file that left the list can never be triggered from the menu.
    m_recentMenu->clear();
    const int n = qMin(kRecentCount, m_recentPaths.size());
    for (int i = 0; i < n; ++i) {
        const QString path = m_recentPaths.at(i);
        QAction* a = m_recentMenu->addAction(QDir::toNativeSeparators(path));
        a->setObjectName(QStringLiteral("actionFileRecent%1").arg(i));
        a->setData(path);
        connect(a, &QAction::triggered, this, [this, path] {
            emit recentFileRequested(path);
        });
    }
    m_recentEntry->setVisible(m_recentsWanted && n > 0);
}

QMenu* MenuBar::addMenu(const QString& title)
{
    auto* m = new BorlandMenu(this);
    m->setTitle(title);
    QMenuBar::addMenu(m);
    return m;
}

QMenu* MenuBar::menu(const QString& title) const
{
    const QString want = title;   // callers pass plain titles ("File");
    for (QAction* a : actions()) {
        if (a->text().remove(QLatin1Char('&')) == want)
            return a->menu();
    }
    return nullptr;
}

bool MenuBar::openMenuByMnemonic(QChar letter)
{
    if (letter.isNull())
        return false;
    const QChar want = letter.toUpper();
    for (QAction* a : actions()) {
        if (!a || !a->menu())
            continue;
        const QString text = a->text();
        if (text.isEmpty())
            continue;
        // An explicit '&' marker wins; otherwise the first letter is the
        // mnemonic (the bar's titles are plain: "File", "Edit", ...).
        QChar mnemonic = text.at(0);
        const int amp = text.indexOf(QLatin1Char('&'));
        if (amp >= 0 && amp + 1 < text.size())
            mnemonic = text.at(amp + 1);
        if (mnemonic.toUpper() != want)
            continue;

        // Drop the menu under its title, exactly where a click lands. The bar
        // is never mapped, so actionGeometry() can still be empty before the
        // first layout - fall back to the left edge when it is.
        // QMenu::popup reads window()->windowHandle()->screen(). Anywhere the
        // window was never shown (tests, a detached bar) that handle is null
        // and the popup SIGSEGVs in QWindow::screen(); force the unmapped
        // handle first, exactly as ContentBridge does for the live shell.
        if (QWidget* top = window())
            if (!top->windowHandle())
                top->winId();
        const QRect r = actionGeometry(a);
        const QPoint at = mapToGlobal(
            QPoint(r.width() > 0 ? r.left() : 0,
                   r.height() > 0 ? r.bottom() + 1 : height()));
        setActiveAction(a);
        a->menu()->popup(at);
        return a->menu()->isVisible();
    }
    return false;
}

QStringList MenuBar::topLevelTitles() const
{
    QStringList out;
    for (QAction* a : actions())
        out << a->text().remove(QLatin1Char('&'));
    return out;
}

// Explicitly fill the menu bar TV light-gray (palette alone was overridden by
// the app-wide window-blue QWidget rule); QMenuBar draws its items on top.
void MenuBar::paintEvent(QPaintEvent* ev)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(0xAA, 0xAA, 0xAA));
    QMenuBar::paintEvent(ev);
}