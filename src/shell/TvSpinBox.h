// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QWidget>
#include <QString>
#include <QRect>

class QLineEdit;

// A Turbo Vision spin box: a blue field showing the value, with two stacked
// green stepper cells (up over down) against its right edge, each with its own
// block shadow. Typing is real: Return commits the entry, clamping it to the
// range, and an unparsable entry snaps back to the current value.
//
// Triangles are painted, not drawn as characters: the bundled VGA face is
// CP437 and does not carry U+25B2/U+25BC.
class TvSpinBox : public QWidget
{
    Q_OBJECT
public:
    explicit TvSpinBox(QWidget* parent = nullptr);

    void setRange(double lo, double hi);
    double minimum() const { return m_min; }
    double maximum() const { return m_max; }
    void setSingleStep(double step) { m_step = qMax(0.0, step); }
    double singleStep() const { return m_step; }
    void setDecimals(int decimals);
    int decimals() const { return m_decimals; }
    void setSuffix(const QString& suffix);
    QString suffix() const { return m_suffix; }

    void setValue(double value);
    double value() const { return m_value; }

    void stepUp();
    void stepDown();
    void commitTyped();

    QLineEdit* editor() const { return m_edit; }
    QRect fieldRect() const { return m_fieldRect; }
    QRect upRect() const { return m_upRect; }
    QRect downRect() const { return m_downRect; }

    QSize sizeHint() const override;

signals:
    void valueChanged(double value);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void resizeEvent(QResizeEvent*) override;

private:
    void layoutCells();
    void refreshText();
    QString formatted() const;
    double clamped(double value) const;

    QLineEdit* m_edit = nullptr;
    double m_min = 0.0;
    double m_max = 100.0;
    double m_step = 1.0;
    double m_value = 0.0;
    int m_decimals = 0;
    QString m_suffix;
    QRect m_fieldRect;
    QRect m_upRect;
    QRect m_downRect;
};
