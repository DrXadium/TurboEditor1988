// SPDX-License-Identifier: GPL-3.0-or-later
#include "BracketColorizer.h"
#include <QStack>

namespace BracketColorizer {

QVector<QColor> defaultColors()
{
    // Kate's Coloured Brackets defaults, verbatim (config group ColoredBrackets).
    return {
        QColor(0x12, 0x75, 0xEF),   // blue
        QColor(0xF8, 0x3C, 0x1F),   // orange-red
        QColor(0x9D, 0xBA, 0x1E),   // olive green
        QColor(0xE2, 0x19, 0xE2),   // magenta
        QColor(0x37, 0xD2, 0x1C),   // green
    };
}

namespace {
bool isOpening(QChar c)
{
    return c == QLatin1Char('{') || c == QLatin1Char('[') || c == QLatin1Char('(');
}
bool isBracket(QChar c)
{
    return isOpening(c) || c == QLatin1Char('}') || c == QLatin1Char(']')
           || c == QLatin1Char(')');
}
// Does `in` close the opener `from`?
bool matches(QChar in, QChar from)
{
    return (from == QLatin1Char('{') && in == QLatin1Char('}'))
           || (from == QLatin1Char('[') && in == QLatin1Char(']'))
           || (from == QLatin1Char('(') && in == QLatin1Char(')'));
}

struct Opener {
    QChar ch;
    int line;
    int col;
};

struct Pair {
    int openLine;
    int openCol;
    int closeLine;
    int closeCol;
};
} // namespace

QVector<Coloured> colorize(const QVector<QString>& lines,
                           int firstLine,
                           int lastLine,
                           const StyleProbe& probe,
                           int colorCount)
{
    QVector<Coloured> out;
    if (lines.isEmpty() || colorCount <= 0)
        return out;

    // Match pairs over the WHOLE document: a pair opened in the viewport may
    // close below it (or above), and a closer only makes sense with its opener.
    // Scan a little above the viewport so an opener just off-screen still pairs.
    // Stack-match. Only pairs whose OPENER sits inside [firstLine, lastLine]
    // are coloured - the viewport decides what is drawn, the whole document
    // decides what pairs.
    QVector<Pair> pairs;
    QStack<Opener> stack;
    for (int l = 0; l < lines.size(); ++l) {
        const QString& line = lines.at(l);
        for (int c = 0; c < line.size(); ++c) {
            const QChar ch = line.at(c);
            if (!isBracket(ch))
                continue;
            if (probe && probe(l, c))
                continue;
            if (isOpening(ch)) {
                stack.push({ ch, l, c });
            } else if (!stack.isEmpty() && matches(ch, stack.top().ch)) {
                const Opener o = stack.pop();
                pairs.append({ o.line, o.col, l, c });
            }
        }
    }

    // Drop a pair whose opener is immediately followed by its closer on the
    // same line ("()" / "{}" with nothing inside): an empty block adds no
    // structural information. This is upstream's col+1==c rule.
    for (int i = 0; i < pairs.size(); ++i) {
        const Pair& p = pairs.at(i);
        if (p.openLine == p.closeLine && p.openCol + 1 == p.closeCol)
            pairs.remove(i--);
    }

    // Colour in document order, wrapping the palette but never repeating the
    // colour of the pair immediately before (the user's rule; upstream guard).
    int color = 0;
    for (const Pair& p : pairs) {
        if (p.openLine < firstLine || p.openLine > lastLine)
            continue;                    // not in the viewport
        if (out.size() > 0 && out.last().color == color)
            color = (color + 1) % colorCount;
        out.append({ p.openLine, p.openCol, color, p.closeCol, p.closeLine });
        color = (color + 1) % colorCount;
    }
    return out;
}

} // namespace BracketColorizer
