// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QColor>
#include <QPushButton>
#include <QRect>
#include <QString>

// A Turbo Vision green plate that behaves like a button should: pressed, it
// turns the bright green and travels down-right ONTO its own block shadow, so
// the shadow disappears under it and the plate reads as sunk into the panel.
//
// The widget's rect is the PLATE plus the shadow band (4 px right and bottom) -
// it paints the shadow itself. That matters for input: the widget never moves,
// so a release always lands on it. Moving the widget instead would leave a
// band where the press hits but the release misses (ContentBridge re-hit-tests
// every synthesized event; there is no implicit mouse grab), which is both a
// dead click and a plate stuck down.
class TvPlate : public QPushButton
{
    Q_OBJECT
public:
    explicit TvPlate(const QString& text, QWidget* parent = nullptr);

    // White ink on the modal plates, black on the file dock's.
    void setInk(const QColor& ink);
    QColor ink() const { return m_ink; }
    // The panel the plate sits on - the band the plate vacates when pressed
    // has to be erased with it.
    void setBackdrop(const QColor& colour);
    QColor backdrop() const { return m_backdrop; }
    // How far the plate travels onto its shadow. Default 4.
    void setShadowOffset(int offset);
    int shadowOffset() const { return m_offset; }

    // The visible plate inside this widget's rect.
    QRect plateRect() const;

protected:
    void paintEvent(QPaintEvent*) override;
    void enterEvent(QEnterEvent*) override;
    void leaveEvent(QEvent*) override;

private:
    QColor m_ink = Qt::white;
    QColor m_backdrop = QColor(0xAA, 0xAA, 0xAA);
    int m_offset = 4;
};
