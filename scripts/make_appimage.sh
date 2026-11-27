#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Build CRT-Kate-x86_64.AppImage.
#   1. configure + build + install into staging/ (prefix /usr)
#   2. fetch linuxdeploy + linuxdeploy-plugin-qt (cached in .tmp/tools)
#   3. stage syntax-highlighting data (Fedora F43 ships NO KSyntaxHighlighting
#      definitions anywhere, so they come from the upstream frameworks repo,
#      pinned to the installed KF version - cached in .tmp/syntax)
#   4. seed an AppDir (bin + share + data), let linuxdeploy bundle Qt/KF6 deps,
#      emit the AppImage
# Must run inside the crt-kate distrobox (Qt/KF6 toolchain present):
#   bash scripts/make_appimage.sh
set -euo pipefail
cd "$(dirname "$0")/.."
ROOT="$PWD"
KFVER="${CRT_KATE_KF_VERSION:-6.30.0}"
JOBS="$(nproc)"

echo "== [1/4] configure + build + install (prefix /usr, DESTDIR staging)"
# Release builds use a SEPARATE build tree (build-release) configured as
# Release, so the developer's Debug tree is never packaged and the artifact
# stays as small as it needs to be.
#
# The prefix-map flags rewrite __FILE__ to a neutral path and
# -gno-record-gcc-switches drops the argv0 line, so no reference to the build
# machine (home directory, project path) is left in the shipped ELF.
BLD="$ROOT/build-release"
PREFIX_FLAGS=(
  "-ffile-prefix-map=$ROOT=."
  "-fdebug-prefix-map=$ROOT=."
  "-gno-record-gcc-switches"
)
cmake -B "$BLD" -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_FLAGS="${PREFIX_FLAGS[*]}" \
  -DCMAKE_CXX_FLAGS="${PREFIX_FLAGS[*]}" \
  >/dev/null
cmake --build "$BLD" -j"$JOBS"
rm -rf staging
DESTDIR="$ROOT/staging" cmake --install "$BLD" >/dev/null

echo "== [2/4] tool downloads (cached)"
TOOLS="$ROOT/.tmp/tools"
mkdir -p "$TOOLS"
fetch() { # fetch <url> <dest>
  if [ ! -x "$2" ]; then
    echo "   downloading $(basename "$2")"
    curl -fsSL --retry 3 -o "$2" "$1"
    chmod +x "$2"
  fi
}
fetch "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage" \
      "$TOOLS/linuxdeploy-x86_64.AppImage"
fetch "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage" \
      "$TOOLS/linuxdeploy-plugin-qt-x86_64.AppImage"
fetch "https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage" \
      "$TOOLS/appimagetool-x86_64.AppImage"

echo "== [3/4] syntax-highlighting definitions + themes (pinned v$KFVER)"
SYNTAX_DIR="$ROOT/.tmp/syntax/syntax-highlighting-v$KFVER"
if [ ! -d "$SYNTAX_DIR/data/syntax" ]; then
  echo "   downloading frameworks/syntax-highlighting v$KFVER"
  mkdir -p "$ROOT/.tmp/syntax"
  curl -fsSL --retry 3 -o "$ROOT/.tmp/syntax/syntax-highlighting-v$KFVER.tar.gz" \
    "https://invent.kde.org/frameworks/syntax-highlighting/-/archive/v$KFVER/syntax-highlighting-v$KFVER.tar.gz"
  tar -xzf "$ROOT/.tmp/syntax/syntax-highlighting-v$KFVER.tar.gz" -C "$ROOT/.tmp/syntax"
fi

echo "== [4/4] seed AppDir + linuxdeploy"
rm -rf AppDir CRT-Kate-*.AppImage
mkdir -p AppDir/usr/share/KSyntaxHighlighting
cp -a staging/usr/. AppDir/usr/
# Upstream data layout: data/syntax/*.xml (definitions) + data/themes/*.theme
mkdir -p AppDir/usr/share/KSyntaxHighlighting/definitions
cp -a "$SYNTAX_DIR/data/syntax/." AppDir/usr/share/KSyntaxHighlighting/definitions/
cp -a "$SYNTAX_DIR/data/themes/." AppDir/usr/share/KSyntaxHighlighting/themes/
# ship a license note next to the bundled data
{ echo "Bundled from https://invent.kde.org/frameworks/syntax-highlighting"
  echo "tag v$KFVER (KSyntaxHighlighting definitions/themes, MIT/BSD licenses)."
} > AppDir/usr/share/KSyntaxHighlighting/UPSTREAM.txt

export PATH="$TOOLS:$PATH"
# The downloaded binary is <name>-x86_64.AppImage (no bare "linuxdeploy" file);
# resolve it explicitly so the lookup never depends on PATH parsing quirks.
LINUXDEPLOY="$TOOLS/linuxdeploy-x86_64.AppImage"
test -x "$LINUXDEPLOY" || { echo "ERROR: $LINUXDEPLOY missing"; exit 1; }
# linuxdeploy's AppImage runtime must work without FUSE inside containers.
export APPIMAGE_EXTRACT_AND_RUN=1
# Make Qt's QML module tree visible to linuxdeploy-plugin-qt so it bundles
# QtQuick/QtQuick.Window (the app imports them at runtime).
export QML2_IMPORT_PATH="${QML2_IMPORT_PATH:-}:/usr/lib64/qt6/qml"
export QML_IMPORT_PATH="${QML_IMPORT_PATH:-}:/usr/lib64/qt6/qml"
export QT_PLUGIN_PATH="${QT_PLUGIN_PATH:-}:/usr/lib64/qt6/plugins"
# linuxdeploy's bundled binutils is too old for Fedora's RELR (.relr.dyn)
# relocations ("unknown type [0x13]") and aborts while stripping QML plugin
# libs. Skipped entirely (larger AppImage, but correct) - upstream workaround.
export NO_STRIP=true

cd "$ROOT"
# Phase A: bundle Qt/KF6 deps into AppDir (no output step yet).
"$LINUXDEPLOY" --appdir AppDir \
  --executable AppDir/usr/bin/turbo-kate \
  --desktop-file AppDir/usr/share/applications/turbo-kate.desktop \
  --icon-file AppDir/usr/share/icons/hicolor/256x256/apps/turbo-kate.png \
  --plugin qt

# linuxdeploy only ships libqxcb.so as a QPA platform plugin; wayland and
# everything else (shell integrations, xcb GL glue, ...) is dlopened and
# invisible to ldd-following. Copy the entire system Qt plugin tree to the
# plugins ROOT (dirname of platforms/), keeping the standard layout intact.
PLUGDIR="$(dirname "$(find AppDir -name libqxcb.so | head -1)")"
PLUGINROOT="$(dirname "$PLUGDIR")"
echo "   bundling full Qt plugin tree into $PLUGINROOT"
cp -a /usr/lib64/qt6/plugins/. "$PLUGINROOT/"

# linuxdeploy's bundled patchelf rewrites DT_RUNPATH on every deployed lib.
# On the small QML plugin ELFs (+40% size growth) combined with Fedora's RELR
# (.relr.dyn) relocations this corrupts relocation targets: dlopen of
# QtQuick.Window's plugin ctor SIGSEGVs.
# The QML plugins do not need patching: the AppRun wrapper below exports
# LD_LIBRARY_PATH=$APPDIR/usr/lib, which regular dlopen resolution honors.
# Restore the pristine system QML tree over the patched copy:
cp -a /usr/lib64/qt6/qml/. AppDir/usr/qml/

# linuxdeploy also prefers a plain AppRun SYMLINK, which exports nothing:
# no QML/data paths, no LD_LIBRARY_PATH for pristine plugins. Replace it with
# a standard shell AppRun. (rm first: cat > SYMLINK would follow it and
# clobber usr/bin/turbo-kate itself.)
rm -f AppDir/AppRun
cat > AppDir/AppRun <<'APPRUN'
#!/bin/sh
# CRT-Kate AppImage launcher: export payload paths, then exec the real binary.
HERE="$(dirname "$(readlink -f "$0")")"
export LD_LIBRARY_PATH="$HERE/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export QML2_IMPORT_PATH="$HERE/usr/qml${QML2_IMPORT_PATH:+:$QML2_IMPORT_PATH}"
export QML_IMPORT_PATH="$QML2_IMPORT_PATH"
export XDG_DATA_DIRS="$HERE/usr/share${XDG_DATA_DIRS:+:$XDG_DATA_DIRS}"
# Bundled TV face lives in $HERE/usr/share/fonts/TTF (see CMake install rule);
# XDG_DATA_DIRS above already exposes it to fontconfig, so the family resolves
# without a custom FONTCONFIG_* override.
exec "$HERE/usr/bin/turbo-kate" "$@"
APPRUN
chmod +x AppDir/AppRun

# Close the DT_NEEDED graph: linuxdeploy skips some system libraries
# (notably the GLVND family libOpenGL/libGLX/libEGL), which only surfaces
# when running on a machine/container without those - so copy every missing
# non-glibc dependency from the host into AppDir ourselves. glibc itself is
# deliberately NOT bundled: AppImages use the target system's libc.
echo "   enforcing library closure (self-contained AppDir)"
mkdir -p AppDir/usr/lib
declare -A seen=()
queue=("AppDir/usr/bin/turbo-kate")
# Seed with every Qt plugin .so (dlopened - not reachable from the binary's
# NEEDED edges) so the closure walk also pulls their dependencies.
while IFS= read -r plug; do queue+=("$plug"); done < <(find "$PLUGINROOT" -name "*.so" -type f)
while [ ${#queue[@]} -gt 0 ]; do
  src="${queue[0]}"
  queue=("${queue[@]:1}")
  [ -n "${seen[$src]:-}" ] && continue
  seen[$src]=1
  while read -r needed; do
    needed="${needed#[}"
    needed="${needed%]}"
    case "$needed" in
      libc.so*|libm.so*|libpthread*|libdl*|librt*|libresolv*|ld-linux*|libnss_*|libutil*) continue ;;
    esac
    if [ ! -e "AppDir/usr/lib/$needed" ]; then
      [ -e "/usr/lib64/$needed" ] || continue  # e.g. vdso or glibc'd elsewhere
      cp -L "/usr/lib64/$needed" "AppDir/usr/lib/$needed"
    fi
    # Always queue: already-bundled libs still need THEIR NEEDEDs scanned.
    queue+=("AppDir/usr/lib/$needed")
  done < <(readelf -d "$src" 2>/dev/null | awk '/NEEDED/{print $NF}')
done
echo "   closure: $(ls AppDir/usr/lib | wc -l) libs"

# Stage 3: restore PRISTINE system copies of every ELF that linuxdeploy's
# bundled patchelf rewrote. It appends a 4th LOAD segment when setting
# DT_RUNPATH, desyncing RELR relocations on Fedora libs: repro was
# LD_LIBRARY_PATH=AppDir/usr/lib -> SIGSEGV 139 inside a QML plugin ctor,
# same libs without LD -> pass (host libs mixed in by accident). Nothing
# needs patching: AppRun exports LD_LIBRARY_PATH/QML/data paths, and the
# qml/plugin trees were already restored to pristine above.
echo "   restoring pristine libs (undo patchelf DT_RUNPATH rewrites)"
restore_pristine() {
  local b d
  b="$(basename "$1")"
  for d in /usr/lib64 /usr/lib64/qt6/plugins /usr/lib64/qt6/qml; do
    if [ -f "$d/$b" ]; then cp -fL "$d/$b" "$1"; return 0; fi
  done
  return 0
}
while IFS= read -r f; do restore_pristine "$f"; done \
  < <(find AppDir/usr -type f \( -name "*.so" -o -name "*.so.*" \))
# crt-kate itself: pristine staging copy (no rpath; AppRun provides lib path).
cp -f "$ROOT/staging/usr/bin/turbo-kate" AppDir/usr/bin/turbo-kate

# Stage 4: pack with appimagetool DIRECTLY. Spawning it without
# APPIMAGE_EXTRACT_AND_RUN is the bisected-good shape (FUSE works in this
# box); a linuxdeploy --output step would just re-patch what we restored.
echo "   packing AppDir with appimagetool"
rm -f CRT-Kate-*x86_64*.AppImage
if ! env -u APPIMAGE_EXTRACT_AND_RUN \
       "$TOOLS/appimagetool-x86_64.AppImage" AppDir "$ROOT/CRT-Kate-x86_64.AppImage" \
       > "$ROOT/.tmp/appimagetool.log" 2>&1; then
  echo "ERROR: appimagetool failed"
  tail -10 "$ROOT/.tmp/appimagetool.log"
  exit 1
fi
[ -s "$ROOT/CRT-Kate-x86_64.AppImage" ] || { echo "ERROR: no AppImage produced"; exit 1; }
echo "OK: $ROOT/CRT-Kate-x86_64.AppImage ($(du -h CRT-Kate-x86_64.AppImage | cut -f1))"