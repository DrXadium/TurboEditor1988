// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "TvModal.h"

class ColorPickerPanel;

// "Color" — the house modal that hosts the full-range picker (2026-11-23).
//
// A TvModal SUBCLASS, like AboutModal and FindReplaceDialog, rather than a bare
// TvModal plus setBody(): the house pattern keeps the body owned by the dialog,
// so the panel cannot outlive it or be deleted twice. TvModal composites as a
// child of the shell and claims the keyboard on Show, which is exactly what
// Xadium asked for ("make sure it draws in window not in a new window").
class ColorPickerModal : public TvModal
{
    Q_OBJECT
public:
    explicit ColorPickerModal(QWidget* parent = nullptr);

    // The colour the picker opens on: seeds the SV square, the hue strip, the
    // hex field, and the preview chip's "before" half.
    void setInitialColor(const QColor& c);
    QColor initialColor() const;

    // What the user is currently pointing at (live, before OK is pressed).
    QColor pickedColor() const;
    ColorPickerPanel* panel() const { return m_panel; }

private:
    ColorPickerPanel* m_panel = nullptr;
    QColor m_initial;
};
