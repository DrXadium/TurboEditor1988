// SPDX-License-Identifier: GPL-3.0-or-later
#include "SwatchApply.h"

#include <ktexteditor/document.h>

bool applySwatchColor(KTextEditor::Document* doc, int line, const HexColorToken& token,
                      const QColor& color)
{
    if (!doc || !color.isValid())
        return false;
    if (line < 0 || line >= doc->lines())
        return false;
    if (token.length <= 0 || token.start < 0)
        return false;

    // The token is compared in its RGB form: a picker colour built from HSV
    // would otherwise never equal the RGB colour already in the text.
    const QColor current = token.color.toRgb();
    const QColor wanted = color.toRgb();
    if (current == wanted)
        return false;

    const QString text = wanted.name(QColor::HexRgb);   // "#rrggbb", lowercase
    doc->replaceText(KTextEditor::Range(line, token.start, line, token.start + token.length),
                     text);
    return true;
}
