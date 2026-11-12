// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QMenu>

// TV-revision2 spec 6 dropdown: a solid light-gray box with NO line-drawing
// border; black item text with a RED hotkey letter; the highlighted row is a
// FULL-WIDTH green bar (edge to edge, black text); a check-mark slot in
// column 1 (text from column 2); shortcuts right-aligned with one cell of
// padding; separators are one full-width single black line.
//
// Why a subclass: QStyleSheetStyle intercepts CE_MenuItem painting whenever the
// app carries a stylesheet that matches the menu widget (the generic QWidget
// rule matches every widget), so BorlandStyle::drawControl(CE_MenuItem) is
// never reached. Owning paintEvent sidesteps QStyle entirely for popups.
class BorlandMenu : public QMenu
{
    Q_OBJECT
public:
    explicit BorlandMenu(QWidget* parent = nullptr);

    // How far the single black border sits inside the panel edge (the gray
    // panel shows outside it on all four sides, per the reference screenshot).
    static int borderInset() { return 5; }

protected:
    void paintEvent(QPaintEvent* ev) override;
};
