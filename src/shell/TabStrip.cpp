// SPDX-License-Identifier: GPL-3.0-or-later
#include "TabStrip.h"
#include <QPainter>

TabStrip::TabStrip(QWidget* parent) : QWidget(parent)
{
    setObjectName("tabStrip");
    setMinimumHeight(24);
}

void TabStrip::setTabs(const QStringList& titles, int active)
{
    m_titles = titles;
    m_active = active;
    update();
}
