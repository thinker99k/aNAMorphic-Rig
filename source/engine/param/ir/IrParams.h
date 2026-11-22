#pragma once

#include "engine/param/common/BlockParams.h"
#include "ValueSteps.h"

#include <vector>

namespace rig
{
    // IR 블록용 L. Cut(Low Cut) / H. Cut(High Cut) 파라미터 정의 (값은 BlockParams, 번호는 Index)
    struct IrParams
    {
        // 범위·기울기 상수는 여기에만 둔다 (UI 도 이 값을 쓴다)
        // 기본값 = 노브 12시 = √(min × max): √(25 × 400) = 100, √(1250 × 20000) = 5000. 전원은 기본 꺼짐이라 새 블록 소리는 그대로
        static constexpr double lowCutMinHz  = 25.0,   lowCutMaxHz  = 400.0,   lowCutDefaultHz  = 100.0;
        static constexpr double highCutMinHz = 1250.0, highCutMaxHz = 20000.0, highCutDefaultHz = 5000.0;

        // 기울기 = Butterworth 차수 x 6 dB/oct. 4 차 = 24 dB/oct = 2차 필터 2 단 (차수는 짝수)
        static constexpr int cutOrder = 4;

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

    static_assert (IrParams::lowCutMaxHz < IrParams::highCutMinHz);   // 400 < 1250: L. Cut 과 H. Cut 이 겹치지 않는다
    static_assert (IrParams::cutOrder > 0 && IrParams::cutOrder % 2 == 0);
}
