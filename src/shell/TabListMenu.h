// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QStringList>
#include <QPoint>
#include <QRect>
#include <QSize>

class QMenu;
class QWidget;

// The tab-list dropdown behind the row's ▼ control (mock note 5): every open
// tab as one entry, styled exactly like the menu bar's dropdowns (BorlandMenu).
// Each action carries its tab index as QAction::data, so the caller only needs
// the triggered action to switch documents.
namespace TabListMenu {

QMenu* build(QWidget* parent, const QStringList& titles, int active);

// Where the dropdown opens: below the ▼ control, RIGHT-aligned to it, and
// clamped so the whole list stays inside `shellGlobal`. Without the clamp a
// 163 px list anchored at the control's left edge runs off the right side of
// the window and the user never sees it (reported live).
QPoint anchoredPos(const QRect& cellGlobal, const QSize& menuSize,
                   const QRect& shellGlobal);

} // namespace TabListMenu
