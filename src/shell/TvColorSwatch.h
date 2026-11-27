// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QColor>
#include <QPushButton>

// A clickable colour chip for the house dialogs: the chosen colour in a hard
// 1 px black frame, no bevel, no QSS plate behind it. Clicking it is the
// caller's cue to open the in-window ColorPickerModal.
class TvColorSwatch : public QPushButton
{
    Q_OBJECT
public:
    explicit TvColorSwatch(QWidget* parent = nullptr);

    void setColor(const QColor& colour);
    QColor color() const { return m_colour; }

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* ev) override;

private:
    QColor m_colour{ 0xFF, 0xFF, 0xFF };
};
