// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QColor>
#include <QString>
#include <QVector>

// One colour code found in a line of text: where it sits and what it means.
// `start` is 0-based, `length` includes the leading '#'.
struct HexColorToken {
    int start = 0;
    int length = 0;
    QColor color;
};

// Every `#rgb` / `#rrggbb` token in `line`, left to right (2026-11-23).
//
// This is the swatch feature's text half: the editor paints a colour box after
// each token, so a wrong range puts a box in the middle of a word or leaves one
// off the end of the line. The rules, in order:
//   - a token starts at '#' and is followed by 6 or 3 hex digits;
//   - a 6-digit run must NOT be followed by another hex digit, so "#abcdef0"
//     yields nothing rather than "#abcdef" plus a stray '0';
//   - 3 digits stand for 6 by doubling each nibble (#abc -> #aabbcc).
// Named colours, rgb()/hsl() and alpha (#rrggbbaa) are deliberately out of
// scope (design doc 2026-11-23): an alpha code stays unswatched rather than
// being shown in the wrong colour.
QVector<HexColorToken> scanHexColors(const QString& line);

// Parses a user-typed colour: "#rgb", "rgb", "#rrggbb" or "rrggbb".
// Leaves `out` untouched and returns false for anything else, so the picker's
// hex field can reject junk without moving its markers.
bool parseHexColor(const QString& text, QColor* out);
