#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Write the release checksum for a built artifact.
#
# Produces "<file>.sha256" containing "<hex>  <basename>" — the exact format
# the updater's Checksum::verifyFile parses (it takes the first whitespace
# token as the digest). Publishing a checksum that does not match the uploaded
# AppImage makes every client's auto-update refuse, silently, so this runs as
# part of the build rather than from memory at release time.
#
# usage: gen_sha256.sh <file>
set -eu

if [ "$#" -ne 1 ]; then
    echo "usage: $(basename "$0") <file>" >&2
    exit 2
fi

file="$1"
if [ ! -f "$file" ]; then
    echo "gen_sha256: no such file: $file" >&2
    exit 1
fi

sum="$(sha256sum "$file" | awk '{print $1}')"
[ -n "$sum" ] || { echo "gen_sha256: sha256sum produced no digest" >&2; exit 1; }

# Write beside the file and rename into place: a half-written checksum must
# never be left where a release step would pick it up.
tmp="$file.sha256.tmp"
trap 'rm -f "$tmp"' EXIT HUP INT TERM
printf '%s  %s\n' "$sum" "$(basename "$file")" > "$tmp"
mv -f "$tmp" "$file.sha256"
trap - EXIT HUP INT TERM

echo "wrote $(basename "$file").sha256 (${sum})"
