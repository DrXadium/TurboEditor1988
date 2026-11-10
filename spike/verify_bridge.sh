#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Task 1 spike gate: build the spike, render one frame, and assert the
# QWidget's solid background color appears in the captured image.
# Prints "BRIDGE OK: <mechanism>" on success; exits non-zero on failure.
set -u
cd "$(dirname "$0")/.."

OUT=spike/out
mkdir -p "$OUT"

echo "[1/3] configure + build"
cmake -S . -B build >"$OUT/cmake.log" 2>&1 || { echo "BUILD FAIL (cmake)"; tail -20 "$OUT/cmake.log"; exit 2; }
cmake --build build --target crt-kate-spike >"$OUT/build.log" 2>&1 || { echo "BUILD FAIL (compile)"; tail -30 "$OUT/build.log"; exit 2; }

echo "[2/3] run spike (capture frame)"
# offscreen falls back gracefully; real GL is available via DISPLAY=:0
QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}" ./build/crt-kate-spike "$OUT/frame.png" \
  >"$OUT/run.log" 2>&1 || { echo "RUN FAIL"; cat "$OUT/run.log"; exit 3; }

echo "[3/3] assert widget color present in frame"
if [ ! -f "$OUT/frame.png" ]; then echo "NO FRAME produced"; exit 4; fi

# Count pure-blue pixels using the proven decoder (spike/pngcolors.py).
# It prints "size WxH ..." then color/count lines; extract the (0,0,255) count.
COLORS=$(python3 spike/pngcolors.py "$OUT/frame.png")
BLUE_COUNT=$(printf '%s\n' "$COLORS" | awk '$1=="(0," && $2=="0," && $3=="255)" {print $4}')
[ -z "$BLUE_COUNT" ] && BLUE_COUNT=0

echo "blue pixels: $BLUE_COUNT"
if [ "${BLUE_COUNT:-0}" -gt 1000 ]; then
  MECH=$(grep -o 'MECHANISM:.*' "$OUT/run.log" | head -1 || echo "MECHANISM: unknown")
  echo "BRIDGE OK: ${MECH#MECHANISM: }"
  exit 0
else
  echo "BRIDGE FAIL: widget background not found in frame"
  exit 1
fi
