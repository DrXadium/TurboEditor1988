// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QColor>

#include "src/shell/HexColorScan.h"

namespace KTextEditor {
class Document;
}

// Writes a picked colour back over the token it came from (2026-11-23).
//
// One `replaceText` over exactly the token's range, so the change is a single
// undo step and the rest of the line stays byte-identical. `token.start` and
// `length` come from scanHexColors() on the same line, so a short `#abc` token
// is rewritten in full six-digit form (the picker works in full range).
//
// Returns false and touches NOTHING when the colour already matches: a no-op
// pick must not dirty a saved file or push an undo step the user never asked
// for.
bool applySwatchColor(KTextEditor::Document* doc, int line, const HexColorToken& token,
                      const QColor& color);
