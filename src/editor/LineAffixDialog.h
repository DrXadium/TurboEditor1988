// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "src/shell/TvModal.h"

#include <QString>

class QLineEdit;
class TvCheckBox;
class FieldCell;

// Edit > Add to End of Line / Add to Start of Line (2026-11-19): the house
// modal that collects the affix. End mode adds a "Line break" checkbox that
// makes the affix a line break instead of the typed string (the field goes
// disabled, so what the dialog will insert is never ambiguous). The dialog
// applies nothing itself - the manager reads text()/wantsLineBreak() after
// TvModal::exec returns Accept, the same division Find/Replace uses.
class LineAffixDialog : public TvModal
{
    Q_OBJECT
public:
    enum class Mode { Start, End };
    Q_ENUM(Mode)

    explicit LineAffixDialog(Mode mode, QWidget* parent = nullptr);

    QLineEdit* field() const { return m_field; }
    TvCheckBox* lineBreakBox() const { return m_box; }
    QString text() const;
    bool wantsLineBreak() const;

private:
    Mode m_mode;
    FieldCell* m_cell = nullptr;
    QLineEdit* m_field = nullptr;
    TvCheckBox* m_box = nullptr;
};
