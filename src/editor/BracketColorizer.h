// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QString>
#include <QVector>
#include <QColor>
#include <functional>

// Coloured Brackets: give each bracket PAIR a colour so nested blocks are
// readable (Kate 25.12's addon, ported - see docs/superpowers/specs/
// 2026-11-20-coloured-brackets-design.md).
//
// The algorithm is deliberately a PURE function so pair-matching and the
// "no two adjacent pairs share a colour" rule are unit tests, not pixel probes.
// The KTextEditor wiring (MovingRanges, scroll/text signals) lives beside it and
// is verified through the renderer.
namespace BracketColorizer {

// A coloured bracket pair's anchor: the line/column of its OPENER (the closer is
// found too, but the caller only needs where to start the moving range - both
// ends are derived from `cols`).
struct Coloured {
    int line = 0;   // line of the opener
    int col = 0;    // column of the opener
    int color = 0;  // index into the colour list
    int closeCol = 0;   // column of the closer (same line as the opener's line
                        // is NOT assumed; see `closeLine`)
    int closeLine = 0;  // line of the closer
};

// Palette: Kate's five defaults, verbatim.
QVector<QColor> defaultColors();

// True when the character at (line, col) is inside a comment or a string and so
// must NOT be coloured (a bracket there is text, not structure).
using StyleProbe = std::function<bool(int line, int col)>;

// Colour the bracket pairs whose openers fall in [firstLine, lastLine] (inclusive,
// 0-based). `lines` is the whole document (a pair may close below the viewport, so
// matching runs to the end; only COLOURING is viewport-scoped, as upstream).
// `probe` reports comment/string positions (always false for pure unit tests).
// Returns openers in document order, each with a colour index in
// [0, colors.size()).
QVector<Coloured> colorize(const QVector<QString>& lines,
                           int firstLine,
                           int lastLine,
                           const StyleProbe& probe,
                           int colorCount);

} // namespace BracketColorizer
