// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "src/shell/TvModal.h"

#include <QString>

class QLineEdit;
class FieldCell;

// View > Go... (2026-11-19): the house modal that collects a line number. The
// field starts EMPTY (Xadium: you rarely want the line you are already on) and
// carries a QIntValidator(1, lineCount) so bad typing is refused at the keyboard.
// line() clamps as well, because a paste can land a value the validator never
// watched. Applies nothing itself - the composition root reads line() after
// TvModal::exec returns Accept, the same division the line-modify dialogs use.
class GoToLineDialog : public TvModal
{
    Q_OBJECT
public:
    // lineCount is the document's line count (1-based): the validator and the
    // clamp both key off it.
    explicit GoToLineDialog(int lineCount, QWidget* parent = nullptr);

    QLineEdit* field() const { return m_field; }
    // 1-based, clamped to [1, lineCount]. Non-numeric input reads as 1.
    int line() const;

private:
    int m_lineCount;
    FieldCell* m_cell = nullptr;
    QLineEdit* m_field = nullptr;
};
