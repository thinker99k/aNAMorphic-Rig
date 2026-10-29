#!/bin/zsh
# aNAMorphic-Rig : macOS 빌드
#
# 실행 (프로젝트 루트에서):
#   ./build.zsh
 
set -eu -o pipefail
cd "${0:A:h}"
 
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build
 