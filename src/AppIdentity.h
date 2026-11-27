// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

// Single source of truth for the application's names.
//
// displayName() is what the user sees: window titles, the About box, menu
// entries and the desktop entry all say "TurboEditor1988" - the canonical
// project name (2026-11-27, Xadium: the release must carry the one name).
//
// storageDirName() is the LEGACY on-disk identity and must never change:
// saved sessions (~/.config/CRT-Kate/sessions) and settings
// (~/.config/CRT-Kate/crt.ini) live under it, and there is no migration. The
// autosave directory is a separate literal (~/.local/share/crt-kate/autosave).
namespace AppIdentity {

inline QString displayName()
{
    return QStringLiteral("TurboEditor1988");
}

inline QString storageDirName()
{
    return QStringLiteral("CRT-Kate");
}

} // namespace AppIdentity
