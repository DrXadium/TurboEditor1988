#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# TurboEditor1988 AppImage swap helper (auto-update).
#
# Run DETACHED by the app after it verifies a downloaded update, then the app
# exits. This script waits for the running AppImage process to disappear,
# re-verifies the new file's SHA-256, swaps it in place (old file kept as
# .bak), and relaunches. Any failure restores the .bak and exits non-zero so
# the old app is never lost.
#
# usage: turbo-kate-update.sh <targetAppImage> <newImage> <expectedSha256>
set -u

target="${1:?target AppImage path required}"
newimg="${2:?new AppImage path required}"
wantsha="${3:?expected sha256 required}"

# Bounded wait for the old process to exit (the file must not be open).
for _ in $(seq 1 100); do
    # fuser -s returns 0 while the file is still open by a process.
    if ! command -v fuser >/dev/null 2>&1 || ! fuser -s "$target" 2>/dev/null; then
        break
    fi
    sleep 0.1
done

# Defense in depth: re-verify before touching the installed file.
gotsha="$(sha256sum "$newimg" 2>/dev/null | awk '{print $1}')"
if [ -z "$gotsha" ] || [ "$gotsha" != "$wantsha" ]; then
    echo "update: checksum mismatch (got '${gotsha:-none}', want '$wantsha')" >&2
    rm -f "$newimg"
    exit 2
fi

bak="$target.bak"
rm -f "$bak"
if ! mv "$target" "$bak"; then
    echo "update: could not move old AppImage aside" >&2
    rm -f "$newimg"
    exit 3
fi
if ! mv "$newimg" "$target"; then
    # Roll back: put the old app exactly where it was.
    mv "$bak" "$target" 2>/dev/null
    echo "update: could not move new AppImage into place" >&2
    exit 4
fi
chmod +x "$target" 2>/dev/null
rm -f "$bak"

exec "$target"
