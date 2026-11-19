// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QPainter>
#include <QRect>
#include <QString>

// Turbo Vision double-line frame rules, drawn at CHARACTER-CELL scale.
//
// A TV frame edge is a whole cell thick: the ═ glyph is two horizontal rules
// with visible daylight between them, centred in the cell band. Drawing two
// ADJACENT 1-px lines (the previous approach) reads as a thin 2-px blob and
// disappears entirely under the CRT pass — that is why the ASCII borders
// "were not there" on screen.
namespace TvFrame {

// Two horizontal rules centred vertically in `band`.
inline void doubleH(QPainter& p, const QRect& band, const QColor& ink)
{
    if (band.width() <= 0 || band.height() <= 0)
        return;
    const int gap = qMax(2, band.height() / 3);   // daylight between the rules
    const int cy = band.top() + band.height() / 2;
    p.fillRect(QRect(band.left(), cy - gap / 2 - 1, band.width(), 1), ink);
    p.fillRect(QRect(band.left(), cy + gap / 2, band.width(), 1), ink);
}

// Two vertical rules centred horizontally in `band`.
inline void doubleV(QPainter& p, const QRect& band, const QColor& ink)
{
    if (band.width() <= 0 || band.height() <= 0)
        return;
    const int gap = qMax(2, band.width() / 3);
    const int cx = band.left() + band.width() / 2;
    p.fillRect(QRect(cx - gap / 2 - 1, band.top(), 1, band.height()), ink);
    p.fillRect(QRect(cx + gap / 2, band.top(), 1, band.height()), ink);
}

// Cell-sized rectangle outline (bracket boxes like [■] and [↕]): two rules per
// side, so the box reads at the same weight as the frame it sits in.
// Daylight between a bracket box's two rules per side.
inline int boxGap(const QRect& r)
{
    return qMax(2, qMin(r.width(), r.height()) / 4);
}

inline void box(QPainter& p, const QRect& r, const QColor& ink)
{
    if (r.width() <= 2 || r.height() <= 2)
        return;
    const int gap = boxGap(r);
    // Top + bottom pairs.
    p.fillRect(QRect(r.left(), r.top(), r.width(), 1), ink);
    p.fillRect(QRect(r.left(), r.top() + gap, r.width(), 1), ink);
    p.fillRect(QRect(r.left(), r.bottom() - gap, r.width(), 1), ink);
    p.fillRect(QRect(r.left(), r.bottom(), r.width(), 1), ink);
    // Left + right pairs.
    p.fillRect(QRect(r.left(), r.top(), 1, r.height()), ink);
    p.fillRect(QRect(r.left() + gap, r.top(), 1, r.height()), ink);
    p.fillRect(QRect(r.right() - gap, r.top(), 1, r.height()), ink);
    p.fillRect(QRect(r.right(), r.top(), 1, r.height()), ink);
}

// The space BETWEEN a bracket box's inner rules - what a checked box fills.
// Derived from the same gap as box(), so the fill can never drift from the
// brackets it sits inside.
inline QRect boxInterior(const QRect& r)
{
    if (r.width() <= 2 || r.height() <= 2)
        return QRect();
    const int inset = boxGap(r) + 1;
    return r.adjusted(inset, inset, -inset, -inset);
}

// Draw one box-drawing character centred in a CHARACTER CELL.
//
// This is the whole point of the glyph helpers: the bundled VGA face is a
// CP437 bitmap font, so U+2551/U+2550/U+255A/U+2518 fill their cell exactly
// and tile seamlessly. Hand-drawn hairlines cannot: two 1-px lines read as a
// thin ghost (measured 2 px per row) and separate corner strokes leave a seam
// instead of a dovetail.
inline void glyph(QPainter& p, const QRect& cell, QChar ch, const QColor& ink)
{
    if (cell.width() <= 0 || cell.height() <= 0)
        return;
    p.save();
    // Crisp 1-bit text: subpixel/LCD antialiasing tints the strokes (measured
    // (255,247,227) and (170,247,255) instead of pure white), which both looks
    // wrong through the CRT pass and breaks the "frame is white" contract.
    QFont f = p.font();
    f.setStyleStrategy(QFont::NoAntialias);
    p.setFont(f);
    p.setPen(ink);
    p.drawText(cell, Qt::AlignCenter, QString(ch));
    p.restore();
}

// Tile `ch` across `band`, one character per `cellW`-wide cell.
inline void tileH(QPainter& p, const QRect& band, int cellW, QChar ch, const QColor& ink)
{
    if (cellW <= 0 || band.width() <= 0 || band.height() <= 0)
        return;
    // Draw the trailing PARTIAL cell too (the painter clips it): stopping at
    // whole cells left up to a cell of bare background, which is what made the
    // ║ column stop short of the bottom corner instead of meeting it.
    //
    // The clip is what makes "partial" mean partial: `glyph()` draws a WHOLE
    // character into its rect and only the QPainter's clip stops it, so without
    // this a band whose width is not a whole number of cells overdraws up to
    // one cell PAST its right edge - the ═ rule then bleeds into the corner
    // cell it is supposed to terminate beside.
    p.save();
    p.setClipRect(band);
    for (int x = band.left(); x <= band.right(); x += cellW)
        glyph(p, QRect(x, band.top(), cellW, band.height()), ch, ink);
    p.restore();
}

// Tile `ch` down `band`, one character per `cellH`-tall cell.
inline void tileV(QPainter& p, const QRect& band, int cellH, QChar ch, const QColor& ink)
{
    if (cellH <= 0 || band.width() <= 0 || band.height() <= 0)
        return;
    // Same trailing-partial-cell rule as tileH, and the same need for a clip.
    p.save();
    p.setClipRect(band);
    for (int y = band.top(); y <= band.bottom(); y += cellH)
        glyph(p, QRect(band.left(), y, band.width(), cellH), ch, ink);
    p.restore();
}

// The whole TV modal frame: \u2554 \u2550 title \u2550 \u2557 across the top,
// \u2551 down both sides, \u255a \u2550 \u255d along the bottom, all in white.
// The title is centred on a cell boundary and clamped so the corner cells are
// never eaten; it is drawn in the painter's current font.
//
// Returns false and draws NOTHING when `f` cannot hold a 4x3-cell box: a
// partial box reads as a broken border, which is worse than none.
inline bool drawDoubleFrame(QPainter& p, const QRect& f, const QString& title,
                            int cellW, int cellH)
{
    if (cellW <= 0 || cellH <= 0 || f.width() < 4 * cellW || f.height() < 3 * cellH)
        return false;
    const QColor ink(Qt::white);
    const QFontMetrics fm(p.font());

    // Top row: \u2554 \u2550 title \u2550 \u2557
    glyph(p, QRect(f.left(), f.top(), cellW, cellH), QChar(0x2554), ink);
    glyph(p, QRect(f.right() - cellW + 1, f.top(), cellW, cellH), QChar(0x2557), ink);
    int titleCells = (fm.horizontalAdvance(title) + cellW - 1) / cellW;
    titleCells = qMin(titleCells, qMax(1, f.width() / cellW - 8));
    const int titleW = (titleCells + 2) * cellW;
    int titleX = f.left() + ((f.width() - titleW) / 2 / cellW) * cellW;
    if (titleX < f.left() + cellW)
        titleX = f.left() + cellW;
    if (titleX > f.left() + cellW)
        tileH(p, QRect(f.left() + cellW, f.top(), titleX - f.left() - cellW, cellH),
              cellW, QChar(0x2550), ink);
    {
        QFont tf = p.font();
        tf.setStyleStrategy(QFont::NoAntialias);
        p.save();
        p.setFont(tf);
        p.setPen(ink);
        p.drawText(QRect(titleX, f.top(), titleW, cellH), Qt::AlignCenter, title);
        p.restore();
    }
    const int afterTitle = titleX + titleW;
    if (afterTitle < f.right() - cellW)
        tileH(p, QRect(afterTitle, f.top(), f.right() - cellW - afterTitle + 1, cellH),
              cellW, QChar(0x2550), ink);

    // Bottom row: \u255a \u2550 \u255d
    glyph(p, QRect(f.left(), f.bottom() - cellH + 1, cellW, cellH), QChar(0x255A), ink);
    glyph(p, QRect(f.right() - cellW + 1, f.bottom() - cellH + 1, cellW, cellH),
          QChar(0x255D), ink);
    tileH(p, QRect(f.left() + cellW, f.bottom() - cellH + 1, f.width() - 2 * cellW, cellH),
          cellW, QChar(0x2550), ink);

    // Side columns: \u2551
    tileV(p, QRect(f.left(), f.top() + cellH, cellW, f.height() - 2 * cellH), cellH,
          QChar(0x2551), ink);
    tileV(p, QRect(f.right() - cellW + 1, f.top() + cellH, cellW, f.height() - 2 * cellH),
          cellH, QChar(0x2551), ink);
    return true;
}

// The hard black block shadow behind a plate (button, dropdown cell): the
// caller paints it FIRST and then the plate over it, so only the offset L
// survives on the right and bottom.
inline void drawPlateShadow(QPainter& p, const QRect& plate, int offset)
{
    if (plate.isNull() || offset <= 0)
        return;
    p.fillRect(plate.translated(offset, offset), Qt::black);
}

} // namespace TvFrame
