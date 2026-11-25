#pragma once

#include "tuner/TunerGauge.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace rig
{
    // Live 아래 줄의 간이 튜너 (화살표형). 상태만 받아서 그린다. 마우스 입력 없음, 기준음 Hz 는 표시하지 않는다
    //   윗줄  : ▶ | | |  음 이름  | | | ◀    | 자리는 고정 (빈 자리는 어둡게), 해당하는 | 하나만 칠한다
    //   아랫줄: 오차 cents (가운데)
    //  - | 는 음 이름 안쪽부터 1번째 = 3 ~ 10 cents, 2번째 = ~ 20, 3번째 = 20 초과 (barCents). 낮으면 왼쪽, 높으면 오른쪽
    //  - 맞음 (|cents| <= TunerGauge::inTuneCents): 음 이름만 초록, | 와 화살표는 꺼짐
    //  - 어긋남: 음 이름 + 해당 | + 어긋난 쪽 화살표 (낮으면 ▶, 높으면 ◀) 를 주황
    //  - 신호 없음 · 튜너 꺼짐: 모두 회색, 음 이름 "-"
    class TunerStrip final : public juce::Component
    {
    public:
        TunerStrip();

        void setState (const TunerState& newState);   // 받을 때마다 repaint (Live 아래 줄 타이머 20 Hz)
        void paint (juce::Graphics&) override;

    private:
        TunerState state;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TunerStrip)
    };
}
