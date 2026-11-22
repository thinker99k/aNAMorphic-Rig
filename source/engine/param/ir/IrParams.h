#pragma once

#include "engine/param/common/BlockParams.h"
#include "ValueSteps.h"

#include <vector>

namespace rig
{
    // IR 블록용 LC(Low Cut) / RC(High Cut) 파라미터 정의 (값은 BlockParams, 번호는 Index)
    struct IrParams
    {
        // 범위·기울기 상수는 여기에만 둔다 (UI 도 이 값을 쓴다)
        static constexpr double lowCutMinHz  = 20.0,   lowCutMaxHz  = 250.0,   lowCutDefaultHz  = 20.0;
        static constexpr double highCutMinHz = 2500.0, highCutMaxHz = 20000.0, highCutDefaultHz = 20000.0;

        // 기울기 = Butterworth 차수 x 6 dB/oct. 8 차 = 48 dB/oct = 2차 필터 4 단 (차수는 짝수)
        static constexpr int cutOrder = 8;

        enum Index : int { lowCutOn, lowCutHz, highCutOn, highCutHz };

        static const std::vector<ParamSpec>& specs()
        {
            static const std::vector<ParamSpec> s {
                { "lowCutOn",  0.0f, 1.0f, 0.0f, 0, true },
                { "lowCutHz",  (float) lowCutMinHz,  (float) lowCutMaxHz,  (float) lowCutDefaultHz,  steps::hzDecimals },
                { "highCutOn", 0.0f, 1.0f, 0.0f, 0, true },
                { "highCutHz", (float) highCutMinHz, (float) highCutMaxHz, (float) highCutDefaultHz, steps::hzDecimals } };
            return s;
        }
    };

    static_assert (IrParams::lowCutMaxHz < IrParams::highCutMinHz);   // 250 < 2500: LC 와 RC 가 겹치지 않는다
    static_assert (IrParams::cutOrder > 0 && IrParams::cutOrder % 2 == 0);
}
