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
    // One cell wide: matches the app's monospace grid (tests pin <= 12 px).
    static constexpr int kCellWidth = 12;

    explicit RightScrollBar(QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* ev) override;
};