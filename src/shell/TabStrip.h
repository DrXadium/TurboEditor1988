// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QWidget>

// Document tab strip (Borland-styled). Populated by EditorTabManager (Task 5).
class TabStrip : public QWidget
{
    Q_OBJECT
public:
    explicit TabStrip(QWidget* parent = nullptr);
    void setTabs(const QStringList& titles, int active);
signals:
    void tabActivated(int index);
private:
    QStringList m_titles;
    int m_active = 0;
};
