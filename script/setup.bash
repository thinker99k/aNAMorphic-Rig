#!/usr/bin/env bash
# aNAMorphic-Rig : Linux 빌드 준비물 설치
#
# 설치하는 것
#   - clang, cmake, ninja, pkg-config
#   - JUCE 리눅스 백엔드가 쓰는 헤더: ALSA, freetype, fontconfig, X11
#     (Wayland 세션에서도 XWayland 로 실행됨)
#
# 지원: Debian/Ubuntu 계열 (apt), Fedora/RHEL 계열 (dnf)
#
# 실행 (프로젝트 루트에서):
#   bash ./script/setup.bash

set -euo pipefail

APT_PACKAGES=(
    clang cmake ninja-build pkg-config
    libasound2-dev libfreetype-dev libfontconfig1-dev
    libx11-dev libxcomposite-dev libxcursor-dev libxext-dev
    libxinerama-dev libxrandr-dev libxrender-dev libxi-dev
)

DNF_PACKAGES=(
    clang cmake ninja-build pkgconf-pkg-config
    alsa-lib-devel freetype-devel fontconfig-devel
    libX11-devel libXcomposite-devel libXcursor-devel libXext-devel
    libXinerama-devel libXrandr-devel libXrender-devel libXi-devel
)

SUDO=""
if [[ $EUID -ne 0 ]]; then
    SUDO="sudo"
fi

# /etc/os-release 의 ID, ID_LIKE 로 배포판 계열 판별
. /etc/os-release
DISTRO="${ID:-} ${ID_LIKE:-}"

if [[ "$DISTRO" =~ (debian|ubuntu) ]]; then
    echo "[apt] Debian/Ubuntu 계열 (${PRETTY_NAME:-unknown})"
    $SUDO apt-get update
    # Ubuntu 22.04 처럼 libfreetype-dev 가 없는 배포판은 옛 이름으로 대체
    if ! apt-cache show libfreetype-dev >/dev/null 2>&1; then
        APT_PACKAGES=("${APT_PACKAGES[@]/libfreetype-dev/libfreetype6-dev}")
    fi
    $SUDO apt-get install -y "${APT_PACKAGES[@]}"
elif [[ "$DISTRO" =~ (fedora|rhel|centos) ]]; then
    echo "[dnf] Fedora/RHEL 계열 (${PRETTY_NAME:-unknown})"
    $SUDO dnf install -y "${DNF_PACKAGES[@]}"
else
    echo "BTW I'm using arch, fuck off please" >&2
    exit 1
fi

echo ""
clang --version | head -n 1
cmake --version | head -n 1
echo "ninja $(ninja --version)"
echo ""
echo "완료."
