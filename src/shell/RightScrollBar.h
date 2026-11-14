// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "TextScrollBar.h"

// TV-revision2 spec 1: the window's RIGHT BORDER COLUMN is the vertical
// scrollbar — exactly one character cell wide, flush to the window's right
// edge, spanning from under the top-right corner to above the bottom-right
// corner. Glyph language: a teal end cell with a blue-ink ▲ / ▼ (blue, not
// black — spec forbids any pure black inside the window), a teal-on-blue ▒
// track, and a SINGLE-CELL solid teal ■ thumb near the top.
// Subclasses TextScrollBar so ScrollBarInstaller's converter leaves it alone
// (it skips TextScrollBar instances).
class RightScrollBar : public TextScrollBar
{
    Q_OBJECT
public:
    explicit RightScrollBar(QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* ev) override;
    void changeEvent(QEvent* ev) override;

private:
    // One CHARACTER CELL wide, measured from the font. The border column has to
    // sit on the frame's character grid or the bottom-right corner glyph cannot
    // dovetail with it: a hardcoded 12 px was one and a half cells at the
    // bundled 16 px VGA face (cell = 8 px), which left the ┘ stem 7 px in from
    // the column's left edge.
    void updateCellWidth();
};