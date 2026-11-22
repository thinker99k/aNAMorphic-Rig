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
        static constexpr double gainMinDb = -18.0, gainMaxDb = 18.0, gainDefaultDb = 0.0;

        // 세 밴드 모두 peak 필터, Freq = 중심 주파수
        static constexpr double bassMinHz   = 40.0,   bassMaxHz   = 200.0,   bassDefaultHz   = 100.0;
        static constexpr double middleMinHz = 315.0,  middleMaxHz = 2000.0,  middleDefaultHz = 800.0;
        static constexpr double trebleMinHz = 2500.0, trebleMaxHz = 10000.0, trebleDefaultHz = 4000.0;

        static constexpr double h1Hz = 250.0;    // Bass / Middle 경계
        static constexpr double h2Hz = 2500.0;   // Middle / Treble 경계

        static constexpr double eqQ = 0.8;       // Bass / Middle / Treble 공통 폭 (약 1.5~2 옥타브)

        enum Index : int { bassDb, bassHz, middleDb, middleHz, trebleDb, trebleHz };

        static const std::vector<ParamSpec>& specs()
        {
            static const std::vector<ParamSpec> s {
                { "bassDb",   (float) gainMinDb,   (float) gainMaxDb,   (float) gainDefaultDb,   steps::dbDecimals },
                { "bassHz",   (float) bassMinHz,   (float) bassMaxHz,   (float) bassDefaultHz,   steps::hzDecimals },
                { "middleDb", (float) gainMinDb,   (float) gainMaxDb,   (float) gainDefaultDb,   steps::dbDecimals },
                { "middleHz", (float) middleMinHz, (float) middleMaxHz, (float) middleDefaultHz, steps::hzDecimals },
                { "trebleDb", (float) gainMinDb,   (float) gainMaxDb,   (float) gainDefaultDb,   steps::dbDecimals },
                { "trebleHz", (float) trebleMinHz, (float) trebleMaxHz, (float) trebleDefaultHz, steps::hzDecimals } };
            return s;
        }
    };

    // 침범 금지: 어떤 조합에서도 Bass Freq < Middle Freq < Treble Freq
    static_assert (NamParams::bassMaxHz   <  NamParams::h1Hz && NamParams::h1Hz < NamParams::middleMinHz);   // 200 < 250 < 315
    static_assert (NamParams::middleMinHz >= NamParams::bassMaxHz   * 1.5);                                  // 최소 간격
    // Middle / Treble 은 순서만 보장한다 (Treble Freq 를 2.5 kHz 부터 쓰기 위해 H2 여유와 1.5 배 간격 규칙은 뺐다)
    static_assert (NamParams::middleMaxHz < NamParams::trebleMinHz);                                         // 2000 < 2500
}
