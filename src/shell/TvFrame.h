// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QColor>
#include <QFont>
#include <QPainter>
#include <QRect>

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
inline void box(QPainter& p, const QRect& r, const QColor& ink)
{
    if (r.width() <= 2 || r.height() <= 2)
        return;
    const int gap = qMax(2, qMin(r.width(), r.height()) / 4);
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
    for (int x = band.left(); x + cellW <= band.right() + 1; x += cellW)
        glyph(p, QRect(x, band.top(), cellW, band.height()), ch, ink);
}

// Tile `ch` down `band`, one character per `cellH`-tall cell.
inline void tileV(QPainter& p, const QRect& band, int cellH, QChar ch, const QColor& ink)
{
    if (cellH <= 0 || band.width() <= 0 || band.height() <= 0)
        return;
    for (int y = band.top(); y + cellH <= band.bottom() + 1; y += cellH)
        glyph(p, QRect(band.left(), y, band.width(), cellH), ch, ink);
}

} // namespace TvFrame
