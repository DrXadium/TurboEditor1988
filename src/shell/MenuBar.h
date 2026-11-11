// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QMenuBar>

// Kate's menu options rendered in Turbo Vision style (Tasks 10 + 12). Builds
// the full top-level menu structure (File, Edit, Search, View, Session,
// Settings, Help) with their entries so the shell/test can assert on it
// standalone. The entries are exposed via menu(title) so the composition root
// (main.cpp) can wire each action to EditorTabManager / SessionManager /
// SettingsDialog. Paints its own light-gray bar (palette alone was overridden
// by the app-wide window-blue QWidget rule).
class MenuBar : public QMenuBar
{
    Q_OBJECT
public:
    explicit MenuBar(QWidget* parent = nullptr);

    // The QMenu for a top-level title (e.g. "File"), or nullptr if absent.
    // Title may include a hotkey marker ("&File"); it is matched after stripping.
    QMenu* menu(const QString& title) const;

    // Titles of the top-level menus, in order (hotkey markers stripped).
    QStringList topLevelTitles() const;

    // Creates a BorlandMenu (self-painted dropdown, spec 6) and adds it.
    // Shadows QMenuBar::addMenu(const QString&) so every top-level menu gets
    // the TV dropdown painting.
    QMenu* addMenu(const QString& title);

protected:
    void paintEvent(QPaintEvent* ev) override;
};
