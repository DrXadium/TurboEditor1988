// SPDX-License-Identifier: GPL-3.0-or-later
#include "FindReplace.h"

namespace FindReplace {
namespace {

// The pattern as the engine uses it. An empty or unparsable pattern comes back
// as a DEFAULT-CONSTRUCTED regex, whose empty pattern the callers reject - a
// default QRegularExpression is otherwise valid and would match the empty
// string at every position.
QRegularExpression compile(const QString& pattern, const Options& options)
{
    if (pattern.isEmpty())
        return QRegularExpression();

    QString source = options.regex ? pattern : QRegularExpression::escape(pattern);
    // KTextEditor treats whole-word matching as plaintext-only, and so do we.
    if (!options.regex && options.wholeWords)
        source = QStringLiteral("\\b(?:%1)\\b").arg(source);

    QRegularExpression::PatternOptions flags = QRegularExpression::NoPatternOption;
    if (!options.caseSensitive)
        flags |= QRegularExpression::CaseInsensitiveOption;

    QRegularExpression re(source, flags);
    return re.isValid() ? re : QRegularExpression();
}

bool usable(const QRegularExpression& re)
{
    return re.isValid() && !re.pattern().isEmpty();
}

} // namespace

QList<Hit> findAll(const QString& text, const QString& pattern, const Options& options)
{
    const QRegularExpression re = compile(pattern, options);
    QList<Hit> hits;
    if (!usable(re))
        return hits;

    // globalMatch advances past an empty match on its own, so a pattern that can
    // match nothing ("^", "a*") terminates instead of looping.
    QRegularExpressionMatchIterator it = re.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        hits.append(Hit{match.capturedStart(), match.capturedLength()});
    }
    return hits;
}

KTextEditor::Cursor cursorAt(const QString& text, int offset)
{
    const int clamped = qBound(0, offset, int(text.size()));
    int line = 0;
    int lineStart = 0;
    for (int i = 0; i < clamped; ++i) {
        if (text.at(i) == QLatin1Char('\n')) {
            ++line;
            lineStart = i + 1;
        }
    }
    return KTextEditor::Cursor(line, clamped - lineStart);
}

QString expandReplacement(const QString& with, const QRegularExpressionMatch& match, bool regex)
{
    // Plain mode: what was typed is what is inserted. "\1" is three characters.
    if (!regex)
        return with;

    QString out;
    out.reserve(with.size());
    for (int i = 0; i < with.size(); ++i) {
        const QChar c = with.at(i);
        if (c != QLatin1Char('\\') || i + 1 >= with.size()) {
            out += c;
            continue;
        }
        const QChar next = with.at(++i);
        const int group = next.isDigit() ? next.digitValue() : 0;
        if (group >= 1 && group <= 9) {
            if (match.isValid() && group <= match.lastCapturedIndex())
                out += match.captured(group);
            // A group that does not exist contributes nothing.
        } else if (next == QLatin1Char('n')) {
            out += QLatin1Char('\n');
        } else if (next == QLatin1Char('t')) {
            out += QLatin1Char('\t');
        } else {
            out += next;   // \\ -> \, and any other \x -> x
        }
    }
    return out;
}

QString applyAll(const QString& text, const QString& pattern, const QString& with,
                 const Options& options, int* replacedCount)
{
    if (replacedCount)
        *replacedCount = 0;

    const QRegularExpression re = compile(pattern, options);
    if (!usable(re))
        return text;

    // Built from the ORIGINAL text, never from the output, so a replacement
    // that contains the pattern cannot rescan itself into a runaway.
    QString out;
    out.reserve(text.size());
    int cursor = 0;
    int count = 0;
    QRegularExpressionMatchIterator it = re.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        out += text.mid(cursor, match.capturedStart() - cursor);
        out += expandReplacement(with, match, options.regex);
        cursor = match.capturedEnd();
        ++count;
    }
    out += text.mid(cursor);

    if (replacedCount)
        *replacedCount = count;
    return out;
}

int nextHitIndex(const QList<Hit>& hits, int fromOffset)
{
    if (hits.isEmpty())
        return -1;
    for (int i = 0; i < hits.size(); ++i) {
        if (hits.at(i).offset >= fromOffset)
            return i;
    }
    return 0;   // wrapped past the end
}

} // namespace FindReplace
