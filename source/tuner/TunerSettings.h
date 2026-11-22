#pragma once

namespace rig
{
    // settings.json 의 "tunerPitch" (A4 기준음, Hz, 소수점 1자리). 값은 0.1 Hz 단위 정수(tenths)로 다룬다: 440.0 Hz -> 4400
    struct TunerSettings
    {
        static constexpr int minTenths     = 4000;   // 400.0 Hz
        static constexpr int maxTenths     = 5000;   // 500.0 Hz
        static constexpr int defaultTenths = 4400;   // 440.0 Hz
        static constexpr int stepTenths    = 1;      // 0.1 Hz

        static int  clampTenths (int tenths) noexcept;           // minTenths ~ maxTenths 로 제한
        static int  load();                                      // 파일 없음, 키 없음, 숫자가 아님 -> defaultTenths
        static void save (int tenths);                           // "tunerPitch" 키만 바꾼다
        static double toHz (int tenths) noexcept { return tenths / 10.0; }
    };
}
