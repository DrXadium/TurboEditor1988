// SPDX-License-Identifier: GPL-3.0-or-later
#include "MenuBar.h"
#include <QPalette>
#include <QColor>
#include <QPainter>

namespace {
// Turbo Vision palette: window blue #1A1AAA desktop, gray #AAAAAA bars,
// yellow #FFFF55 text, red #C41E1E hotkey — mirrors borland.qss so the
// widget's own palette agrees with the stylesheet visuals (tests assert
// on the palette directly).
void applyBorlandPalette(QWidget* w)
{
    QPalette pal = w->palette();
    const QColor windowBlue(0x1A, 0x1A, 0xAA);
    const QColor gray(0xAA, 0xAA, 0xAA);
    const QColor black(0, 0, 0);
    const QColor yellow(0xFF, 0xFF, 0x55);
    const QColor red(0xC4, 0x1E, 0x1E);
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
    added(edit, QStringLiteral("&UPPERCASE"), QStringLiteral("actionEditUpper"));
    added(edit, QStringLiteral("&lowercase"), QStringLiteral("actionEditLower"));
    added(edit, QStringLiteral("&Sort Lines"), QStringLiteral("actionEditSort"));
    edit->addSeparator();
    QAction* spell = added(edit, QStringLiteral("Check Spelling &On-the-fly"),
                           QStringLiteral("actionEditSpellcheck"));
    spell->setCheckable(true);

    QMenu* search = addMenu(QStringLiteral("Search"));
    added(search, QStringLiteral("&Find..."), QStringLiteral("actionSearchFind"));
    added(search, QStringLiteral("Find &Next"), QStringLiteral("actionSearchFindNext"));

    QMenu* view = addMenu(QStringLiteral("View"));
    added(view, QStringLiteral("Zoom &In"), QStringLiteral("actionViewZoomIn"));
    added(view, QStringLiteral("Zoom &Out"), QStringLiteral("actionViewZoomOut"));
    added(view, QStringLiteral("&Reset Zoom"), QStringLiteral("actionViewZoomReset"));
    view->addSeparator();
    QAction* minimap = added(view, QStringLiteral("Show &Minimap"),
                             QStringLiteral("actionViewMinimap"));
    minimap->setCheckable(true);
    minimap->setChecked(true);
    QAction* foldBar = added(view, QStringLiteral("Show &Folding Bar"),
                             QStringLiteral("actionViewFoldBar"));
    foldBar->setCheckable(true);
    foldBar->setChecked(true);

    QMenu* session = addMenu(QStringLiteral("Se&ssion"));
    added(session, QStringLiteral("&Save Session..."), QStringLiteral("actionSessionSave"));
    added(session, QStringLiteral("Restore &Last Session"), QStringLiteral("actionSessionRestore"));

    QMenu* settings = addMenu(QStringLiteral("Se&ttings"));
    added(settings, QStringLiteral("&Configure CRT-Kate..."),
          QStringLiteral("actionSettingsConfigure"));

    QMenu* help = addMenu(QStringLiteral("&Help"));
    added(help, QStringLiteral("&About CRT-Kate"), QStringLiteral("actionHelpAbout"));
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