// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QString>
#include <QVector>
#include <functional>

// "Which block OPENS on this line?" (2026-11-20).
// See docs/superpowers/specs/2026-11-20-block-context-menu-design.md.
//
// Why it exists: the right-click menu's Collapse/Expand Block commands and the
// block highlight need the extent of the block under the caret, and Kate exposes
// NO public fold-region API - only the actions folding_toplevel,
// folding_expandtoplevel, folding_toggle_current, folding_toggle_in_current and a
// handful of config keys. So the extent is computed here.
//
// PURE by design: no widgets, no KTextEditor, so every rule is a unit test rather
// than a pixel probe - the same shape as BracketColorizer and StringColorizer.
namespace BlockFinder {

// One foldable region. Both ends inclusive; endCol is the position just past the
// closing delimiter (so a Range built from it covers the closer).
struct Block {
    bool valid = false;
    int startLine = 0;
    int startCol = 0;   // where the opening delimiter begins
    int endLine = 0;
    int endCol = 0;
};

// True where the highlighter owns the text (a string or a comment). Those
// positions can never open a block: a '{' in a comment and a '<div>' inside a
// string are text, not structure.
using SkipProbe = std::function<bool(int line, int col)>;

// The block that opens on `line`, or an invalid Block when there is none.
//
// Rules (each one a test in tst_blockfinder):
//  - Openers are '{', '(' , '[' and HTML container elements.
//  - The LAST opener on the line whose closer is on a LATER line wins, so
//    `int main(void) {` folds the brace body rather than the argument list,
//    while a call like `foo(a,\n b\n)` still folds on its paren.
//  - A block must span at least two lines: a single-line `{ x(); }` is not one,
//    and Kate does not fold it either.
//  - An opener that never closes opens nothing.
//  - HTML: nesting is counted by tag name; self-closing tags and void elements
//    never open; <!DOCTYPE> and comments never open.
Block findBlockStartingAt(const QVector<QString>& lines, int line,
                          const SkipProbe& skip);

} // namespace BlockFinder
