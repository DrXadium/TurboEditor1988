// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QStatusBar>

// Bottom F-key / status bar (Borland: F1-Help ... F10-Menu).
class FKeyStatusBar : public QStatusBar
{
    Q_OBJECT
public:
    explicit FKeyStatusBar(QWidget* parent = nullptr);
    void setFKeys(const QString& text);
};
