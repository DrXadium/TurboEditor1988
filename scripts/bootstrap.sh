#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# CRT-Kate dev toolchain bootstrap (Fedora 42, x86_64).
# Lists and installs the Qt6 + KF6 devel packages the build needs.
# This script does NOT run itself: invoke it explicitly (it prompts for sudo).
set -euo pipefail

PKGS=(
  qt6-qtbase-devel
  qt6-qtdeclarative-devel
  qt6-qtshadertools-devel
  qt6-qtbase-private-devel
  qt6-qtdeclarative-private-devel
  kf6-ktexteditor-devel
  kf6-kparts-devel
  kf6-kconfig-devel
  kf6-kcoreaddons-devel
  kf6-kwidgetsaddons-devel
  kf6-kguiaddons-devel
  kf6-kiconthemes-devel
  kf6-kfilemetadata-devel
)

echo "CRT-Kate bootstrap: installing ${#PKGS[@]} packages (Fedora 42, x86_64)"
echo "Packages: ${PKGS[*]}"
echo

# Drop any package dnf does not recognize so a single unavailable package
# (kf6-kfilemetadata-devel is the likely one) does not abort the install.
AVAILABLE=()
for p in "${PKGS[@]}"; do
  if dnf list --available "$p" >/dev/null 2>&1; then
    AVAILABLE+=("$p")
  else
    echo "SKIP (not available in repos): $p"
  fi
done

echo "Installing: ${AVAILABLE[*]}"
sudo dnf install -y "${AVAILABLE[@]}"

echo
echo "Post-install verification:"
qmake6 -query QT_VERSION
test -d /usr/include/KF6/KTextEditor && echo "KF6/KTextEditor headers: present" || { echo "KF6/KTextEditor headers: MISSING"; exit 1; }
echo "Bootstrap OK"
