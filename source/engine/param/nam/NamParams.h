#pragma once

#include "engine/param/common/BlockParams.h"
#include "ValueSteps.h"

#include <vector>

namespace rig
{
    // NAM 블록용 3밴드 EQ 파라미터 정의 (값은 BlockParams, 번호는 Index). EQ 는 항상 켜져 있다 (0 dB = 그대로)
    struct NamParams
    {
        // 범위·경계선·폭 상수는 여기에만 둔다 (UI 도 이 값을 쓴다)
        static constexpr double gainMinDb = -12.0, gainMaxDb = 12.0, gainDefaultDb = 0.0;

        static constexpr double h1Hz = 400.0;    // Bass / Middle 경계
        static constexpr double h2Hz = 2500.0;   // Middle / Treble 경계

        // 세 밴드 모두 peak 필터, Freq = 중심 주파수. 범위 끝 = 경계 (Treble 위는 기타 앰프 소리가 거의 없어 10 kHz 까지)
        // 기본값 = 노브 12시 = 로그 범위의 가운데 √(min × max). 범위는 이 값이 깔끔하게 떨어지도록 잡았다
        // (√(25 × 400) = 100, √(400 × 2500) = 1000, √(2500 × 10000) = 5000)
        static constexpr double bassMinHz   = 25.0, bassMaxHz   = h1Hz,    bassDefaultHz   = 100.0;
        static constexpr double middleMinHz = h1Hz, middleMaxHz = h2Hz,    middleDefaultHz = 1000.0;
        static constexpr double trebleMinHz = h2Hz, trebleMaxHz = 10000.0, trebleDefaultHz = 5000.0;

        static constexpr double eqQ = 0.8;       // Bass / Treble 폭 (고정, 약 1.7 옥타브)

        // Middle 폭 Q (로그). 12시 = √(0.2 × 3.2) = 0.8 = 기본값 = Bass / Treble 과 같은 폭. 0.2 = 약 4.7 옥타브, 3.2 = 약 0.45 옥타브
        // 낮추면 Middle 이 H1 아래 / H2 위로 넘친다 (허용: 경계는 중심 주파수 순서만 지킨다)
        static constexpr double middleQMin = 0.2, middleQMax = 3.2, middleQDefault = 0.8;

        enum Index : int { bassDb, bassHz, middleDb, middleHz, middleQ, trebleDb, trebleHz };

        static const std::vector<ParamSpec>& specs()
        {
            static const std::vector<ParamSpec> s {
                { "bassDb",   (float) gainMinDb,   (float) gainMaxDb,   (float) gainDefaultDb,   steps::dbDecimals },
                { "bassHz",   (float) bassMinHz,   (float) bassMaxHz,   (float) bassDefaultHz,   steps::hzDecimals },
                { "middleDb", (float) gainMinDb,   (float) gainMaxDb,   (float) gainDefaultDb,   steps::dbDecimals },
                { "middleHz", (float) middleMinHz, (float) middleMaxHz, (float) middleDefaultHz, steps::hzDecimals },
                { "middleQ",  (float) middleQMin,  (float) middleQMax,  (float) middleQDefault,  steps::qDecimals },
                { "trebleDb", (float) gainMinDb,   (float) gainMaxDb,   (float) gainDefaultDb,   steps::dbDecimals },
                { "trebleHz", (float) trebleMinHz, (float) trebleMaxHz, (float) trebleDefaultHz, steps::hzDecimals } };
            return s;
        }
    };

    // 침범 금지: 어떤 조합에서도 Bass Freq <= H1 <= Middle Freq <= H2 <= Treble Freq
    static_assert (NamParams::bassMaxHz <= NamParams::h1Hz && NamParams::h1Hz <= NamParams::middleMinHz);     // 400 = 400 = 400
    static_assert (NamParams::middleMaxHz <= NamParams::h2Hz && NamParams::h2Hz <= NamParams::trebleMinHz);   // 2500 = 2500 = 2500
    static_assert (NamParams::bassMinHz < NamParams::bassDefaultHz && NamParams::bassDefaultHz < NamParams::bassMaxHz);
    static_assert (NamParams::middleMinHz < NamParams::middleDefaultHz && NamParams::middleDefaultHz < NamParams::middleMaxHz);
    static_assert (NamParams::trebleMinHz < NamParams::trebleDefaultHz && NamParams::trebleDefaultHz < NamParams::trebleMaxHz);
    static_assert (NamParams::middleQMin < NamParams::middleQDefault && NamParams::middleQDefault < NamParams::middleQMax);
}
