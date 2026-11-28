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

# Stage the new image NEXT TO the target. $newimg lives in $TMPDIR, usually a
# different filesystem: `mv` from there is a non-atomic copy+unlink that could
# leave a half-written target if it fails. A copy into the target's own
# directory makes the final swap a same-directory rename (atomic).
staged="$target.new"
rm -f "$staged"
if ! cp "$newimg" "$staged" 2>/dev/null; then
    rm -f "$staged"
    echo "update: could not stage the new AppImage" >&2
    rm -f "$newimg"
    exit 5
fi
# Verify the STAGED copy: guards a truncated or failed copy.
stagedsha="$(sha256sum "$staged" 2>/dev/null | awk '{print $1}')"
if [ -z "$stagedsha" ] || [ "$stagedsha" != "$wantsha" ]; then
    rm -f "$staged" "$newimg"
    echo "update: staged copy failed verification" >&2
    exit 6
fi
rm -f "$newimg"

bak="$target.bak"
rm -f "$bak"
if ! mv "$target" "$bak"; then
    echo "update: could not move old AppImage aside" >&2
    rm -f "$staged"
    exit 3
fi
if ! mv "$staged" "$target"; then
    # Roll back: put the old app exactly where it was.
    mv "$bak" "$target" 2>/dev/null
    rm -f "$staged"
    echo "update: could not move new AppImage into place" >&2
    exit 4
fi
chmod +x "$target" 2>/dev/null
rm -f "$bak"

# Test seam: skip the relaunch so a fixture (a fake AppImage that isn't a real
# executable) isn't exec'd; production always relaunches.
if [ "${CRT_KATE_UPDATE_NO_RELAUNCH:-0}" = "1" ]; then
    exit 0
fi

exec "$target"
