// SPDX-License-Identifier: GPL-3.0-or-later
#include "FoldIndicator.h"

namespace FoldIndicator {

QVector<int> foldableLines(const QVector<QString>& lines,
                           const BlockFinder::SkipProbe& skip)
{
    QVector<int> result;
    result.reserve(lines.size());
    for (int line = 0; line < lines.size(); ++line) {
        const BlockFinder::Block b =
            BlockFinder::findBlockStartingAt(lines, line, skip);
        // valid + spans at least two lines: a one-line `{ x(); }` is not foldable
        // (Kate does not fold it either), and an opener that never closes is not
        // a block at all.
        if (b.valid && b.endLine > b.startLine)
            result.append(line);
    }
    return result;
}

} // namespace FoldIndicator
