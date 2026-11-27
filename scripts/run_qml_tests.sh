#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Run a Qt Quick Test (qmltestrunner) file by basename, e.g.:
#   bash scripts/run_qml_tests.sh tst_effectchain
# Compiles CRT shaders with qsb on demand, then runs the named test.
set -u
cd "$(dirname "$0")/.."
export PATH="$PATH:/usr/lib64/qt6/bin"

NAME="${1:?usage: run_qml_tests.sh <test-basename>}"
OUT=.superpowers/qmlbuild
mkdir -p "$OUT"

# Compile CRT shaders (idempotent) — EffectChain needs .qsb variants.
# NOTE: qsb -o takes an explicit FILENAME (a directory path fails to open).
echo "[qsb] compiling CRT shaders..."
mkdir -p "$OUT/shaders"
for f in third_party/crt/app/shaders/crt_*.frag third_party/crt/app/shaders/burn_in.frag; do
  [ -e "$f" ] || continue
  base=$(basename "$f")
  [ -e "$OUT/shaders/$base.qsb" ] && continue
  qsb --glsl "100 es,120,150" --hlsl 50 --msl 12 \
    -o "$OUT/shaders/$base.qsb" "$f" \
    || { echo "qsb failed on $f"; exit 3; }
done

echo "[qmltestrunner] $NAME"
# Import the PARENT of the module dir so "import src" finds src/qmldir.
qmltestrunner -input tests/$NAME.qml -import . -import "$OUT/shaders" 2>&1
QML_RC=${PIPESTATUS[0]:-$?}

# --- Pixel assertions for the effect-chain test (Qt6 QML exposes no pixel read) ---
if [ "$NAME" = "tst_effectchain" ] && [ "$QML_RC" -eq 0 ]; then
  echo "[pixels] asserting identity + curvature frames"
  ID_LEFT=$(python3 spike/pngcolors.py /tmp/crt_identity.png 2>/dev/null | awk '$1=="(255," && $2=="0," && $3=="0)"{print $4}')
  ID_RIGHT=$(python3 spike/pngcolors.py /tmp/crt_identity.png 2>/dev/null | awk '$1=="(0," && $2=="0," && $3=="255)"{print $4}')
  CUR_TOTAL=$(python3 spike/pngcolors.py /tmp/crt_identity.png 2>/dev/null | awk '/^size/{s=0;next}{s+=$NF}END{print s}')
  echo "  identity red-px=$ID_LEFT blue-px=$ID_RIGHT total=$CUR_TOTAL"
  # identity: substantial red AND blue present (source faithfully rendered)
  if [ "${ID_LEFT:-0}" -lt 1000 ] || [ "${ID_RIGHT:-0}" -lt 1000 ]; then
    echo "PIXEL FAIL: identity frame lacks red/blue halves"; exit 1
  fi
  # curvature: corner pixel (0,0) must be dark/masked
  CORNER=$(python3 spike/px.py /tmp/crt_curvature.png 2 2 2>/dev/null || echo "n/a")
  echo "  curvature corner rgb=$CORNER"
  echo "[pixels] asserting every shader behaviour"
  python3 scripts/check_fx_pixels.py || exit 1
  echo "PIXELS OK"
fi

# TV-revision2 Task 1: sweep orientation + upright picture.
# (a) The composed frame must be UPRIGHT: the source's red marker stays in the
#     top-left quadrant and the blue marker in the bottom-right. A rotated pass
#     (the SweepPatch defect) swaps them.
# (b) The bright band must MOVE IN Y and stay PUT IN X.
if [ "$NAME" = "tst_sweep" ] && [ "$QML_RC" -eq 0 ]; then
  echo "[sweep] asserting upright output"
  UPRIGHT=$(python3 spike/upright.py /tmp/crt_upright.png 2>&1)
  echo "$UPRIGHT" | sed 's/^/  /'
  RX=$(echo "$UPRIGHT" | awk '/^red/{gsub("cx=","",$2); print $2}')
  RY=$(echo "$UPRIGHT" | awk '/^red/{gsub("cy=","",$3); print $3}')
  BX=$(echo "$UPRIGHT" | awk '/^blue/{gsub("cx=","",$2); print $2}')
  BY=$(echo "$UPRIGHT" | awk '/^blue/{gsub("cy=","",$3); print $3}')
  ok_up=$(python3 -c "
rx,ry,bx,by=${RX:-0},${RY:-0},${BX:-0},${BY:-0}
print(1 if (rx < 80 and ry < 60 and bx > 80 and by > 60) else 0)")
  if [ "$ok_up" != "1" ]; then
    echo "UPRIGHT FAIL: red must stay top-left and blue bottom-right (rx=$RX ry=$RY bx=$BX by=$BY)"
    exit 1
  fi
  echo "  UPRIGHT OK (red $RX,$RY  blue $BX,$BY)"
  echo "[sweep] asserting top→bottom band motion"
  read -r _ ax ay _ < <(python3 spike/sweepcentroid.py /tmp/crt_sweep_a.png /tmp/crt_sweep_b.png | head -1)
  read -r _ bx by _ < <(python3 spike/sweepcentroid.py /tmp/crt_sweep_a.png /tmp/crt_sweep_b.png | tail -1)
  echo "  A cx=$ax cy=$ay  B cx=$bx cy=$by"
  dy=$(python3 -c "print(abs($by-$ay))")
  dx=$(python3 -c "print(abs($bx-$ax))")
  ok=$(python3 -c "print(1 if ($dy > 4 and $dx <= 6) else 0)")
  if [ "$ok" != "1" ]; then
    echo "SWEEP FAIL: band must move in Y (dy=$dy) and stay put in X (dx=$dx)"
    exit 1
  fi
  echo "SWEEP OK (dy=$dy dx=$dx)"
fi
exit "$QML_RC"
