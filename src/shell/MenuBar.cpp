// SPDX-License-Identifier: GPL-3.0-or-later
#include "MenuBar.h"
#include "BorlandMenu.h"
#include "src/AppIdentity.h"
#include <QPalette>
#include <QColor>
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
    file->addSeparator();
    added(file, QStringLiteral("&Save"), QStringLiteral("actionFileSave"));
    added(file, QStringLiteral("Save &As..."), QStringLiteral("actionFileSaveAs"));

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
    edit->addSeparator();
    added(edit, QStringLiteral("&UPPERCASE"), QStringLiteral("actionEditUpper"));
    added(edit, QStringLiteral("&lowercase"), QStringLiteral("actionEditLower"));
    added(edit, QStringLiteral("&Title Case"), QStringLiteral("actionEditTitle"));
    // Line tools: their own group, flat like every other TV menu. Sort Lines
    // heads it; the three new items follow (2026-11-19).
    edit->addSeparator();
    added(edit, QStringLiteral("&Sort Lines"), QStringLiteral("actionEditSort"));
    added(edit, QStringLiteral("Add to &End of Line..."), QStringLiteral("actionEditAddEol"));
    added(edit, QStringLiteral("Add to &Start of Line..."), QStringLiteral("actionEditAddBol"));
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
    QAction* foldBar = added(view, QStringLiteral("Show &Folding Bar"),
                             QStringLiteral("actionViewFoldBar"));
    foldBar->setCheckable(true);
    foldBar->setChecked(true);
    foldBar->setStatusTip(QStringLiteral("Toggle the code folding bar"));
    // 2026-11-19: wrap is per view and ON by default (applyTvStyle). The check
    // follows the active tab, re-synced in the composition root.
    QAction* wrap = added(view, QStringLiteral("&Word Wrap"),
                          QStringLiteral("actionViewWordWrap"));
    wrap->setCheckable(true);
    wrap->setChecked(true);
    wrap->setStatusTip(QStringLiteral("Wrap long lines to the window width"));
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