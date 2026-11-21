// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QString>
#include <QVector>
#include "BlockFinder.h"

// Which lines carry the gutter fold marker? (2026-11-20).
// See docs/superpowers/specs/2026-11-20-fold-indicator-design.md.
//
// The `▶`/`▼` sits on lines that OPEN a foldable block. Folding needs a block
// spanning >= 2 lines (Kate does not fold a one-liner), a block that actually
// closes, and an opener the highlighter owns (not a '{' inside a string or
// comment). Those rules already live in BlockFinder; this applies them to
// EVERY line and returns the qualifying ones.
//
// PURE by design: no widgets, no KTextEditor - each rule is a unit test
// rather than a pixel probe, the same shape as BlockFinder.
namespace FoldIndicator {

// Sorted line numbers (ascending) that open a block spanning >= 2 lines.
// A line qualifies when findBlockStartingAt finds a valid Block whose
// endLine > startLine. Single-line blocks, unmatched openers and skipped
// (string/comment) positions are excluded.
QVector<int> foldableLines(const QVector<QString>& lines,
                           const BlockFinder::SkipProbe& skip);

} // namespace FoldIndicator
