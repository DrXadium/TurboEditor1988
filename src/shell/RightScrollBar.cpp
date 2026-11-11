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

    // Track between the end cells: teal-on-blue ▒ dither (4 px check steps).
    QRect track(0, cellH, w, height() - 2 * cellH);
    p.save();
    p.setPen(Qt::NoPen);
    const int step = 4;
    for (int y = track.top(); y <= track.bottom(); y += step)
        for (int x = track.left(); x <= track.right(); x += step) {
            const int ox = ((y / step) % 2) ? step / 2 : 0;
            p.setBrush(((x / step) + (y / step)) % 2 ? kTeal : kBlue);
            p.drawRect(QRect(x + ox, y, step, step));
        }
    p.restore();

    // End cells: solid teal + blue glyph ink.
    p.fillRect(topCell, kTeal);
    p.fillRect(botCell, kTeal);
    drawArrowBlue(p, topCell, Qt::UpArrow);
    drawArrowBlue(p, botCell, Qt::DownArrow);

    // Thumb: ONE character cell of solid teal, value-proportional in the
    // track, resting near the top at value 0.
    const int thumbLen = cellH;
    const int span = track.height() - thumbLen;
    const int range = maximum() - minimum();
    const int pos = range > 0 ? (value() - minimum()) * span / range : 0;
    p.fillRect(QRect(track.left(), track.top() + pos, w, thumbLen), kTeal);
}