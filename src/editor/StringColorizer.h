// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QString>
#include <QVector>
#include <QColor>
#include <functional>

// String colouring that does NOT depend on the syntax mode (2026-11-20).
// See docs/superpowers/specs/2026-11-20-string-colouring-design.md.
//
// Why it exists: quoted text is coloured by the KSyntaxHighlighting theme, so a
// document whose mode is None (a .txt, an extension-less file, a new untitled
// tab) shows no colour at all. This scans the text itself and colours quoted
// spans through the same MovingRange mechanism the brackets use.
//
// PURE by design: no widgets, no KTextEditor, so the scanning rules (escapes,
// same-line closure, skipping already-styled text) are unit tests rather than
// pixel probes.
namespace StringColorizer {

// One coloured segment of a quoted span, both ends inclusive. A span that
// contains the OTHER quote kind is split into non-overlapping segments (an outer
// range covering an inner one would leave which colour wins to range ordering),
// so segments come back in document order and never overlap.
struct Span {
    int line = 0;
    int col = 0;      // first character of the segment
    int endCol = 0;   // last character (inclusive, same line as `col`)
    QColor color;
};

// The theme's own String/Char green, so a coloured document and an uncoloured
// one look identical where the theme does apply.
QColor defaultColor();

// Single-quoted text gets its own shade (2026-11-20, user: "the text in single
// quotes should be #40A769") so a nested pair like "he said 'hi'" separates into
// two visible colours instead of one green run. Contrast on #0000AA is 11.41.
QColor singleQuoteColor();

// True when the character at (line, col) already has a colour the highlighter
// owns (String, Char or Comment). Those positions are left alone so this pass
// never fights the theme, and so a quoted pair inside a comment stays a comment.
using StyleProbe = std::function<bool(int line, int col)>;

// Quoted spans whose OPENING quote falls in [firstLine, lastLine] (inclusive,
// 0-based). A quote must close on its own line; an unterminated quote opens
// nothing, which is what keeps prose like "don't" from colouring a text file.
// A backslash escapes the next character, so "a\"b" is one span.
QVector<Span> findSpans(const QVector<QString>& lines,
                        int firstLine,
                        int lastLine,
                        const StyleProbe& probe);

} // namespace StringColorizer
