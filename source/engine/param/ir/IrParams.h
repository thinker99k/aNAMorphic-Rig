#pragma once

#include "engine/param/common/BlockParams.h"

#include <atomic>

namespace rig
{
    // IR 블록용 LC(Low Cut) / RC(High Cut) 값 (패치당 1벌, 체인의 모든 IR 블록 뒤에 각각 적용)
    class IrParams final : public BlockParams
    {
    public:
        // 범위·기울기 상수는 여기에만 둔다 (UI 도 이 값을 쓴다)
        static constexpr double lowCutMinHz  = 20.0,   lowCutMaxHz  = 800.0,   lowCutDefaultHz  = 20.0;
        static constexpr double highCutMinHz = 2000.0, highCutMaxHz = 20000.0, highCutDefaultHz = 20000.0;

        // 기울기 = Butterworth 차수 x 6 dB/oct. 8 차 = 48 dB/oct = 2차 필터 4 단 (차수는 짝수)
        static constexpr int cutOrder = 8;

        std::atomic<bool> lowCutOn { false }, highCutOn { false };
        std::atomic<float> lowCutHz  { (float) lowCutDefaultHz };
        std::atomic<float> highCutHz { (float) highCutDefaultHz };

        juce::var toVar() const override;
        void fromVar (const juce::var&) override;
        void setDefaults() override;
    };

    static_assert (IrParams::lowCutMaxHz < IrParams::highCutMinHz);   // 800 < 2000: LC 와 RC 가 겹치지 않는다
    static_assert (IrParams::cutOrder > 0 && IrParams::cutOrder % 2 == 0);
}
