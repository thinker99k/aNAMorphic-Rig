#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace rig
{
    struct TunerState
    {
        bool        hasSignal = false;
        double      hz        = 0.0;
        const char* note      = "";     // "A", "C#" ...
        double      cents     = 0.0;    // -50 ~ +50

        bool operator== (const TunerState&) const = default;
    };

    // 튜너 반원 게이지 (Figma "Tuner 설계"). 상태만 받아서 그린다. 마우스 입력 없음
    //  - 도면 좌표는 404 x 254 기준 (TunerGauge.cpp 의 상수), 크기가 달라도 비율대로 늘린다
    //  - No signal: 바늘/부채꼴 없음, 회색 / In tune (|cents| <= 3): 초록 + IN TUNE 배지 / 그 밖: 주황
    class TunerGauge final : public juce::Component
    {
    public:
        static constexpr double inTuneCents = 3.0;

        TunerGauge();

        void setState (const TunerState& newState);   // 바뀐 경우에만 repaint
        void paint (juce::Graphics&) override;

    private:
        TunerState state;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TunerGauge)
    };
}
