#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Review Focus #5 / plan Task 11: launch the AppImage inside a MINIMAL sandbox
# rootfs that has NO Qt and NO KF6 installed, assert the editor window appears
# and the CRT effect chain is active.
#
# Mechanism:
#   1. dnf --installroot ->.tmp/minroot: bash + glibc + mesa only (cached).
#      (NO qt6*/kf6* packages: verifies the AppImage is self-contained.)
#   2. bwrap sandbox: the ONLY visible root is that rootfs; /dev, /proc, /tmp
#      and the host Wayland socket are passed through so Qt can paint.
#   3. run TurboEditor1988-x86_64.AppImage with CRT_KATE_SHOT= (grabs the live CRT
#      window after 1.5s and exits 0) - real display, never offscreen.
#   4. appimage_shot_assert: many distinct colors (raw widgets are ~5) and
#      thousands of bright pixels (KTextEditor's white surface) => CRT + editor.
set -euo pipefail
cd "$(dirname "$0")/.."
ROOT="$PWD"
IMG="$ROOT/TurboEditor1988-x86_64.AppImage"
MINROOT="$ROOT/.tmp/minroot"
SHOT=/tmp/crt_appimage_check.png

[ -f "$IMG" ] || { echo "ERROR: $IMG missing - run scripts/make_appimage.sh first"; exit 2; }

echo "== [1/4] minimal rootfs (no Qt, no KF6)"
if [ ! -x "$MINROOT/bin/sh" ]; then
  mkdir -p "$MINROOT"
  # dnf5 needs explicit repo definitions inside an installroot (it does not
  # inherit the host's repo config by default).
  sudo dnf install -y --nogpgcheck --installroot="$MINROOT" --releasever=43 \
    --repofrompath=fedora,https://download.fedoraproject.org/pub/fedora/linux/releases/43/Everything/x86_64/os/ \
    --repofrompath=updates,https://download.fedoraproject.org/pub/fedora/linux/updates/43/Everything/x86_64/ \
    --disablerepo='*' --enablerepo=fedora --enablerepo=updates \
    --setopt=install_weak_deps=False --setopt=keepcache=False \
    bash glibc-minimal-langpack coreutils xkeyboard-config fontconfig \
    mesa-dri-drivers mesa-libGL mesa-libEGL mesa-libgbm libglvnd 2>&1 | tail -6
fi
if rpm --root "$MINROOT" -qa 2>/dev/null | grep -qiE "kf6|qt6"; then
  echo "ERROR: rootfs unexpectedly contains Qt/KF6"; exit 1
fi
echo "   rootfs OK (no qt6*/kf6* packages)"

echo "== [2/4] compile shot assertion helper"
g++ -std=c++17 -fPIC "$ROOT/scripts/appimage_shot_assert.cpp" \
    -o "$ROOT/.tmp/appimage_shot_assert" \
    $(pkg-config --cflags --libs Qt6Gui)

echo "== [3/4] sandboxed launch (bwrap, no Qt in rootfs)"
echo "== [3/4] sandboxed launch (bwrap, no Qt in rootfs)"
# /opt is root-owned inside the sudo-built rootfs; stage the AppImage there.
sudo mkdir -p "$MINROOT/opt" "$MINROOT/tmp"
sudo cp -f "$IMG" "$MINROOT/opt/$(basename "$IMG")"
sudo chmod -R a+rwX "$MINROOT/opt" "$MINROOT/tmp"
rm -f "$SHOT"
XDGDIR="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"
# New root = fresh tmpfs (always writable for bwrap's mountpoint creation);
# the minimal rootfs contributes ONLY /usr + /etc (no Qt, no KF6 inside).
# /bin /lib /lib64 /sbin are symlinks into /usr, AppImage + app binaries and
# libs resolve from the AppDir payload. Isolate user/net/ipc/uts; keep the
# host PID namespace so /proc/self/exe works for --appimage-extract-and-run.
EXTRA=()
if [ -d /dev/dri ]; then EXTRA+=(--bind /dev/dri /dev/dri); fi
set +e  # report the sandbox exit code, don't abort mid-diagnostic
bwrap --unshare-user --unshare-net --unshare-ipc --unshare-uts --die-with-parent \
  --ro-bind "$MINROOT/usr" /usr \
  --ro-bind "$MINROOT/etc" /etc \
  --symlink usr/bin /bin \
  --symlink usr/lib /lib \
  --symlink usr/lib64 /lib64 \
  --symlink usr/sbin /sbin \
  --proc /proc \
  --dev /dev \
  "${EXTRA[@]}" \
  --bind /tmp /tmp \
  --bind "$MINROOT/opt" /opt \
  --ro-bind "$XDGDIR" "$XDGDIR" \
  --setenv XDG_RUNTIME_DIR "$XDGDIR" \
  --setenv WAYLAND_DISPLAY "${WAYLAND_DISPLAY:-wayland-0}" \
  --setenv DISPLAY "${DISPLAY:-}" \
  --setenv HOME /tmp \
  --setenv APPIMAGE_EXTRACT_AND_RUN 1 \
  --setenv CRT_KATE_SHOT "$SHOT" \
  --chdir /opt \
  "/opt/$(basename "$IMG")"
LAUNCH=$?
set -e
echo "   launch exit=$LAUNCH"
[ -f "$SHOT" ] || { echo "ERROR: no screenshot produced (launch exit=$LAUNCH)"; exit 1; }

echo "== [4/4] assert CRT active + editor visible"
"$ROOT/.tmp/appimage_shot_assert" "$SHOT" 200 1000
echo "PASS: AppImage runs on a system with no Qt/KF6 installed"