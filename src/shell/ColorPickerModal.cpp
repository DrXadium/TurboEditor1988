// SPDX-License-Identifier: GPL-3.0-or-later
#include "ColorPickerModal.h"
#include "ColorPickerPanel.h"

ColorPickerModal::ColorPickerModal(QWidget* parent) : TvModal(parent)
{
    setTitle(QStringLiteral("Color"));
    m_panel = new ColorPickerPanel(this);
    setBody(m_panel);

    // Plates are laid out right to left, so OK lands rightmost - the same order
    // the other house dialogs use.
    addButton(QStringLiteral("Cancel"), Reject);
    addButton(QStringLiteral("OK"), Accept);
}

void ColorPickerModal::setInitialColor(const QColor& c)
{
    if (!c.isValid())
        return;
    m_initial = c;
    if (m_panel) {
        m_panel->setOriginalColor(c);
        m_panel->setColor(c);
        m_panel->resize(m_panel->sizeHint().expandedTo(m_panel->minimumSize()));
    }
}

QColor ColorPickerModal::initialColor() const
{
    return m_initial;
}

QColor ColorPickerModal::pickedColor() const
{
    return m_panel ? m_panel->color() : QColor();
}
