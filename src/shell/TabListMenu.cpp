// SPDX-License-Identifier: GPL-3.0-or-later
#include "TabListMenu.h"
#include "BorlandMenu.h"
#include <QAction>

namespace TabListMenu {

QMenu* build(QWidget* parent, const QStringList& titles, int active)
{
    auto* menu = new BorlandMenu(parent);
    // Same styling path as the menu bar's dropdowns: a BorlandMenu is painted
    // by its own paintEvent, so no per-menu stylesheet is needed.
    menu->setToolTipsVisible(false);
    for (int i = 0; i < titles.size(); ++i) {
        QAction* a = menu->addAction(titles.at(i));
        a->setData(i);
        a->setCheckable(true);
        a->setChecked(i == active);
    }
    return menu;
}

} // namespace TabListMenu
