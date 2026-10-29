#!/usr/bin/env bash
# aNAMorphic-Rig : Linux 빌드
#
# 실행 (프로젝트 루트에서):
#   ./build.bash

set -euo pipefail
cd "$(dirname "$0")"

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build
