// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QStringList>

class QMenu;
class QWidget;

// The tab-list dropdown behind the row's ▼ control (mock note 5): every open
// tab as one entry, styled exactly like the menu bar's dropdowns (BorlandMenu).
// Each action carries its tab index as QAction::data, so the caller only needs
// the triggered action to switch documents.
namespace TabListMenu {

QMenu* build(QWidget* parent, const QStringList& titles, int active);

} // namespace TabListMenu
