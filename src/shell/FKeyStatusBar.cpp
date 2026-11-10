// SPDX-License-Identifier: GPL-3.0-or-later
#include "FKeyStatusBar.h"

FKeyStatusBar::FKeyStatusBar(QWidget* parent) : QStatusBar(parent)
{
    setObjectName("fkeyStatusBar");
    setFKeys("F1-Help  F2-Save  F3-Open  F9-Compile  F10-Menu");
}

void FKeyStatusBar::setFKeys(const QString& text)
{
    showMessage(text);
}
