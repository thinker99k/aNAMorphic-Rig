#pragma once

#include <cmath>

namespace rig::steps
{
    // 값 단위 (노브·슬라이더 조작과 파일 저장 모두): dB 는 소수 1자리, Hz 는 정수, Q 는 소수 2자리
    constexpr int dbDecimals = 1;
    constexpr int hzDecimals = 0;
    constexpr int qDecimals = 2;

    // 소수 decimals 자리로 반올림. 저장할 때 float 꼬리(3.299999952...)가 남지 않게 double 로 계산한다
    inline double roundTo (double value, int decimals)
    {
        const double scale = std::pow (10.0, decimals);
        return std::round (value * scale) / scale;
    }
}
