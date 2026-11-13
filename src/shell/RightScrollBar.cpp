// SPDX-License-Identifier: GPL-3.0-or-later
#include "RightScrollBar.h"
#include <QPainter>

namespace {
// Palette copy of the TextScrollBar constants (those live in that TU's
// anonymous namespace; kept local to avoid cross-TU coupling).
const QColor kTeal(0x00, 0xAA, 0xAA);
const QColor kBlue(0x00, 0x00, 0xAA);      // track base + arrow glyph ink
} // namespace

// Solid triangle arrow in WINDOW-BLUE ink (never black: spec 1 allows zero
// pure-black pixels inside the window; blue on teal stays legible).
static void drawArrowBlue(QPainter& p, const QRect& r, Qt::ArrowType dir)
{
    p.save();
    p.setPen(Qt::NoPen);
    p.setBrush(kBlue);
    QPolygon tri;
    const int cx = r.center().x(), cy = r.center().y();
    const int hw = r.width() / 2 - 1, hh = r.height() / 2 - 1;
    switch (dir) {
    case Qt::UpArrow:
        tri << QPoint(cx, r.top() + 1) << QPoint(cx - hw, cy + hh / 2)
            << QPoint(cx + hw, cy + hh / 2);
        break;
    default: // Down
        tri << QPoint(cx, r.bottom() - 1) << QPoint(cx - hw, cy - hh / 2)
            << QPoint(cx + hw, cy - hh / 2);
        break;
    }
    p.drawPolygon(tri);
    p.restore();
}

RightScrollBar::RightScrollBar(QWidget* parent)
    : TextScrollBar(Qt::Vertical, parent)
{
    setObjectName(QStringLiteral("rightScrollBar"));
    setFixedWidth(kCellWidth);
}

void RightScrollBar::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    // Character-cell sized end blocks (1 wide x 1 cell tall).
    const int cellH = qMax(8, fontMetrics().height());
    const int w = width();

    QRect topCell(0, 0, w, cellH);
    QRect botCell(0, height() - cellH, w, cellH);

    // Base: window blue everywhere (dither underlayer; no black anywhere).
    p.fillRect(rect(), kBlue);

    // Track between the end cells: teal-on-blue ▒ fine stipple (1 px).
    QRect track(0, cellH, w, height() - 2 * cellH);
    p.save();
    p.setPen(Qt::NoPen);
    // ▒ = an EVEN 1-px teal-on-blue stipple (classic medium-shade glyph
    // texture: fine dots), dithered in BOTH axes. Alternating on x alone left
    // continuous teal bands on a narrow vertical bar - it read as stripes.
    for (int y = track.top(); y <= track.bottom(); ++y)
        for (int x = track.left(); x <= track.right(); ++x)
            if ((x + y) & 1)
                p.fillRect(QRect(x, y, 1, 1), kTeal);
    p.restore();

    // End cells: solid teal + blue glyph ink.
    p.fillRect(topCell, kTeal);
    p.fillRect(botCell, kTeal);
    drawArrowBlue(p, topCell, Qt::UpArrow);
    drawArrowBlue(p, botCell, Qt::DownArrow);

    // Thumb: ONE character cell of teal (the U+2588 BLOCK body), with the
    // U+25A0 SQUARE drawn INSIDE it - the real IDE's "grabby" handle is a
    // character with an inner square, not a plain solid slab.
    const int thumbLen = cellH;
    const int span = track.height() - thumbLen;
    const int range = maximum() - minimum();
    const int pos = range > 0 ? (value() - minimum()) * span / range : 0;
    const QRect thumb(track.left(), track.top() + pos, w, thumbLen);
    p.fillRect(thumb, kTeal);

    // Inner square: centred, inset by a quarter of the cell on every side, in
    // the same blue ink as the arrow glyphs.
    const int inW = qMax(2, w / 2);
    const int inH = qMax(2, thumbLen / 2);
    const int inX = thumb.center().x() - inW / 2;
    const int inY = thumb.center().y() - inH / 2;
    p.fillRect(QRect(inX, inY, inW, inH), kBlue);
}