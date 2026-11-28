// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QString>

// Numeric comparison of release versions for the auto-update check.
//
// Releases are tagged "v0.1.4"; the built-in version is a bare "0.1.3". A tag
// may also carry a git-describe suffix ("v0.1.4-2-gabc123" or "-dirty") when a
// build came from a dirty/untagged tree. Comparison is segment-by-segment and
// NUMERIC (0.1.10 > 0.1.9), ignoring a leading "v" and any "-suffix" on either
// side, so a described build of the same tag never reads as newer than itself.
namespace VersionCompare {

// True only when candidate > current. Equal, older, or unparseable => false.
bool isNewer(const QString& candidate, const QString& current);

} // namespace VersionCompare
