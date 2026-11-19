// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QWidget>
#include <QString>
#include <QRect>

// A Turbo Vision checkbox: a bracketed box followed by the label, drawn the way
// the style engine cannot - the box is the frame's own double rule and the
// cross is two 1 px diagonals. The whole row is the hit target (TV dialogs are
// pointer-generous), Space toggles, and toggled() fires only on a real change.
class TvCheckBox : public QWidget
{
    Q_OBJECT
public:
    explicit TvCheckBox(const QString& text, QWidget* parent = nullptr);

    bool isChecked() const { return m_checked; }
    void setChecked(bool on);
    QString text() const { return m_text; }

    QRect boxRect() const { return m_boxRect; }
    QSize sizeHint() const override;

signals:
    void toggled(bool on);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void resizeEvent(QResizeEvent*) override;

private:
    void layoutBox();

    QString m_text;
    bool m_checked = false;
    QRect m_boxRect;
};
