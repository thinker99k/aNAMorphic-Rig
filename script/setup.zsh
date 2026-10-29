#!/bin/zsh
# aNAMorphic-Rig : macOS 빌드 준비물 설치
#
# 설치하는 것
#   - Xcode Command Line Tools: Apple clang, make, git, macOS SDK
#   - CMake, Ninja: 공식 바이너리를 ~/.local 에 설치 (brew 사용 안 함, sudo 필요 없음)
#
# 실행 (프로젝트 루트에서):
#   zsh ./setup.zsh
#
# 지우려면: rm -rf ~/.local/opt/cmake-* ~/.local/bin/cmake ~/.local/bin/ninja

set -eu -o pipefail

CMAKE_VER=4.3.0
NINJA_VER=1.13.2

PREFIX="$HOME/.local"
BIN="$PREFIX/bin"
OPT="$PREFIX/opt"

# 1. Xcode Command Line Tools
if ! xcode-select -p >/dev/null 2>&1; then
    echo "[install] Xcode Command Line Tools"
    xcode-select --install
    echo "설치 창이 떴습니다. 설치가 끝나면 이 스크립트를 다시 실행하세요."
    exit 0
fi
echo "[skip] Xcode Command Line Tools : 이미 설치됨"

mkdir -p "$BIN" "$OPT"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

# 2. CMake
CMAKE_DIR="$OPT/cmake-${CMAKE_VER}-macos-universal"
if [[ -x "$CMAKE_DIR/CMake.app/Contents/bin/cmake" ]]; then
    echo "[skip] CMake $CMAKE_VER : 이미 설치됨"
else
    echo "[install] CMake $CMAKE_VER"
    curl -fL -o "$TMP/cmake.tar.gz" \
        "https://github.com/Kitware/CMake/releases/download/v${CMAKE_VER}/cmake-${CMAKE_VER}-macos-universal.tar.gz"
    tar xf "$TMP/cmake.tar.gz" -C "$OPT"
fi
ln -sf "$CMAKE_DIR/CMake.app/Contents/bin/cmake" "$BIN/cmake"

# 3. Ninja
if [[ -x "$BIN/ninja" ]] && [[ "$("$BIN/ninja" --version)" == "$NINJA_VER" ]]; then
    echo "[skip] Ninja $NINJA_VER : 이미 설치됨"
else
    echo "[install] Ninja $NINJA_VER"
    curl -fL -o "$TMP/ninja.zip" \
        "https://github.com/ninja-build/ninja/releases/download/v${NINJA_VER}/ninja-mac.zip"
    unzip -o -q "$TMP/ninja.zip" -d "$BIN"
    chmod +x "$BIN/ninja"
fi

# 4. PATH (이미 있으면 추가하지 않음)
PATH_LINE='export PATH="$HOME/.local/bin:$PATH"'
if ! grep -qxF "$PATH_LINE" "$HOME/.zshrc" 2>/dev/null; then
    echo "$PATH_LINE" >> "$HOME/.zshrc"
    echo "[path] ~/.zshrc 에 ~/.local/bin 추가"
fi

echo ""
"$BIN/cmake" --version | head -n 1
echo "ninja $("$BIN/ninja" --version)"
echo ""
echo "완료. 터미널을 새로 열면 cmake, ninja 를 바로 쓸 수 있습니다."
