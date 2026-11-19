// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QList>
#include <QString>
#include <QRegularExpression>
#include <ktexteditor/cursor.h>

// The Find and Replace matching engine: pure functions over text, no widgets.
//
// Matching is QRegularExpression in both modes (a plain pattern is escaped), so
// plain text, whole words and regex share one code path - and the semantics can
// be proved on a QString without a document in sight.
namespace FindReplace {

struct Options {
    bool caseSensitive = false;   // the dialog's default: case-INsensitive
    bool wholeWords = false;      // ignored when `regex` is on
    bool regex = false;
};

struct Hit {
    int offset = 0;
    int length = 0;
};

// Every match, left to right, including zero-length ones (which are real
// matches - "^" inserts a prefix - but cannot be selected in the editor).
// An empty or unparsable pattern yields no hits; it never throws.
QList<Hit> findAll(const QString& text, const QString& pattern, const Options& options);

// Offset -> KTextEditor cursor, so a hit can become a Range.
KTextEditor::Cursor cursorAt(const QString& text, int offset);

// The replacement for one hit: \1..\9 capture refs, \\ for a backslash, \n and
// \t for their control characters. With regex off the text is inserted
// literally (a "\1" the user typed is not a capture reference).
QString expandReplacement(const QString& with, const QRegularExpressionMatch& match, bool regex);

// Replace-all in ONE pass over the ORIGINAL text (never its own output).
// Returns the whole new text; `replacedCount` may be null.
QString applyAll(const QString& text, const QString& pattern, const QString& with,
                 const Options& options, int* replacedCount);

// Index of the first hit starting at or after `fromOffset`, wrapping to 0;
// -1 when there is nothing to find.
int nextHitIndex(const QList<Hit>& hits, int fromOffset);

} // namespace FindReplace
