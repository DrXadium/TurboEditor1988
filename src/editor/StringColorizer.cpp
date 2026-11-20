// SPDX-License-Identifier: GPL-3.0-or-later
#include "StringColorizer.h"

namespace StringColorizer {

namespace {

// The quote that can appear nested inside a string of the given kind.
QChar otherQuote(QChar q)
{
    return q == QLatin1Char('"') ? QLatin1Char('\'') : QLatin1Char('"');
}

QColor colourFor(QChar quote)
{
    return quote == QLatin1Char('\'') ? singleQuoteColor() : defaultColor();
}

// Find the closing `quote` at or after `from`, honouring backslash escapes.
// Returns -1 when the line ends first.
int findClose(const QString& text, int from, QChar quote, int limit)
{
    int i = from;
    while (i < limit) {
        const QChar ch = text.at(i);
        if (ch == QLatin1Char('\\')) {
            i += 2;                 // the escaped character cannot close anything
            continue;
        }
        if (ch == quote)
            return i;
        ++i;
    }
    return -1;
}

// Walk one quoted span starting at `open` and append its coloured segments.
// Returns the index just past the closing quote, or `open + 1` when the span
// never closes (an unterminated quote colours nothing).
int scanSpan(const QString& text, int line, int open, QVector<Span>& out)
{
    const QChar quote = text.at(open);
    const int close = findClose(text, open + 1, quote, text.size());
    if (close < 0)
        return open + 1;

    // The span [open..close] may hold runs of the OTHER quote kind; each becomes
    // its own segment in the other colour, so the two are told apart at a glance.
    const QChar nested = otherQuote(quote);
    int segStart = open;
    int i = open + 1;
    while (i < close) {
        const QChar ch = text.at(i);
        if (ch == QLatin1Char('\\')) {
            i += 2;
            continue;
        }
        if (ch == nested) {
            const int nestedClose = findClose(text, i + 1, nested, close);
            if (nestedClose >= 0) {
                if (i > segStart)
                    out.append(Span{line, segStart, i - 1, colourFor(quote)});
                out.append(Span{line, i, nestedClose, colourFor(nested)});
                segStart = nestedClose + 1;
                i = nestedClose + 1;
                continue;
            }
        }
        ++i;
    }
    if (segStart <= close)
        out.append(Span{line, segStart, close, colourFor(quote)});
    return close + 1;
}

} // namespace

QColor defaultColor()
{
    // The theme's own String/Char green (crt_tv.theme), so a document that HAS a
    // syntax mode looks the same as one that does not.
    return QColor(QStringLiteral("#55ff55"));
}

QColor singleQuoteColor()
{
    return QColor(QStringLiteral("#40a769"));
}

QVector<Span> findSpans(const QVector<QString>& lines,
                        int firstLine,
                        int lastLine,
                        const StyleProbe& probe)
{
    QVector<Span> out;
    if (lines.isEmpty())
        return out;

    // Clamp the viewport; only the OPENING quote is scoped (a span never crosses
    // a line, so the closer is always on the same line).
    const int from = qMax(0, firstLine);
    const int to = qMin(lines.size() - 1, lastLine);
    if (from > to)
        return out;

    for (int line = from; line <= to; ++line) {
        const QString& text = lines.at(line);
        int i = 0;
        while (i < text.size()) {
            const QChar quote = text.at(i);
            if (quote != QLatin1Char('"') && quote != QLatin1Char('\'')) {
                ++i;
                continue;
            }
            // Leave text the highlighter already owns alone: with a syntax mode
            // the theme colours it, and a quote inside a comment stays a comment.
            if (probe && probe(line, i)) {
                ++i;
                continue;
            }
            i = scanSpan(text, line, i, out);
        }
    }
    return out;
}

} // namespace StringColorizer
