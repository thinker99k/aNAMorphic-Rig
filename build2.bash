#!/usr/bin/env bash
# aNAMorphic-Rig : Linux 빌드 결과를 지우고 처음부터 다시 빌드
#
# build/ 안의 내용(숨김 파일 포함)을 모두 지운다. build/ 폴더 자체는 남긴다.
# 그 다음 build 스크립트를 실행한다.
#
# 실행 (프로젝트 루트에서):
#   ./build2.bash

set -euo pipefail
cd "$(dirname "$0")"

if [[ -d build ]]; then
    find build -mindepth 1 -delete
    echo "build/ 안의 내용을 모두 지웠습니다."
fi

exec bash ./build.bash
