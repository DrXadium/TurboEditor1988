// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QMenuBar>

// Kate's menu options rendered in Borland style (see Task 10 for content).
// For Task 3 it provides the menu bar chrome; entries are wired in Task 10.
class MenuBar : public QMenuBar
{
    Q_OBJECT
public:
    explicit MenuBar(QWidget* parent = nullptr);
};
