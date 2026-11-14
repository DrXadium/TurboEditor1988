// SPDX-License-Identifier: GPL-3.0-or-later
#include "TabListMenu.h"
#include "BorlandMenu.h"
#include <QAction>

namespace TabListMenu {

QMenu* build(QWidget* parent, const QStringList& titles, int active)
{
    // Parent to the TOP-LEVEL widget, not to the caller's widget.
    //
    // The caller passes the TabStrip, which sits inside the editor host's
    // QVBoxLayout. A QMenu is a window whose geometry is in global coordinates,
    // so a popup parented to a laid-out child is a trap: as soon as it is forced
    // non-native (WA_DontShowOnScreen, which the popup registrar does) its
    // position becomes parent-relative and the compositor's global-to-shell
    // conversion lands it in the wrong place. Parenting to the window keeps the
    // popup independent of the layout it was opened from.
    auto* menu = new BorlandMenu(parent ? parent->window() : nullptr);
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

QPoint anchoredPos(const QRect& cellGlobal, const QSize& menuSize,
                   const QRect& shellGlobal)
{
    // Right edge on the control's right edge, one pixel under the row.
    int x = cellGlobal.right() + 1 - menuSize.width();
    int y = cellGlobal.bottom() + 1;
    // Clamp: a list wider or taller than the room left must still be fully
    // visible, so pin it to the near edge instead of letting it hang off.
    const int maxX = shellGlobal.left() + shellGlobal.width() - menuSize.width();
    const int maxY = shellGlobal.top() + shellGlobal.height() - menuSize.height();
    x = qBound(shellGlobal.left(), x, qMax(shellGlobal.left(), maxX));
    y = qBound(shellGlobal.top(), y, qMax(shellGlobal.top(), maxY));
    return QPoint(x, y);
}

} // namespace TabListMenu
