// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QStatusBar>

// Bottom F-key / status line (Turbo Vision: F2 Save ... F10 Menu), keys red.
// Paints its own light-gray bar (palette alone was overridden by the app-wide
// window-blue QWidget rule).
class FKeyStatusBar : public QStatusBar
{
    Q_OBJECT
public:
    explicit FKeyStatusBar(QWidget* parent = nullptr);
    void setFKeys(const QString& text);
    QString fkeys() const { return m_text; }

protected:
    void paintEvent(QPaintEvent* ev) override;

private:
    QString m_text;
};
