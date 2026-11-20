// SPDX-License-Identifier: GPL-3.0-or-later
#include "StringColorizer.h"

namespace StringColorizer {

QColor defaultColor()
{
    // The theme's own String/Char green (crt_tv.theme), so a document that HAS a
    // syntax mode looks the same as one that does not.
    return QColor(QStringLiteral("#55ff55"));
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
            // Find the closing quote on THIS line. A backslash escapes the next
            // character, so "a\"b" is one span and "c:\\" closes normally.
            int close = i + 1;
            bool closed = false;
            while (close < text.size()) {
                const QChar ch = text.at(close);
                if (ch == QLatin1Char('\\')) {
                    close += 2;          // skip the escaped character
                    continue;
                }
                if (ch == quote) {
                    closed = true;
                    break;
                }
                ++close;
            }
            if (!closed) {
                // An unterminated quote opens nothing: this is what keeps prose
                // ("don't", "it's") from colouring the rest of a text file.
                ++i;
                continue;
            }
            Span span;
            span.line = line;
            span.col = i;
            span.endCol = close;
            span.color = defaultColor();
            out.append(span);
            i = close + 1;               // continue after the closing quote
        }
    }
    return out;
}

} // namespace StringColorizer
