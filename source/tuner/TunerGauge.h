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
    };

    // 튜너 반원 게이지 (Logic Pro 튜너 배치). 상태만 받아서 그린다. 마우스 입력 없음
    //  - 위 = cents 눈금 호와 바늘, 아래 줄 = [기준음 Hz] [음 이름] [오차 cents]
    //  - 기준음 Hz 는 대화상자의 값 상자가 pitchArea 자리에 겹쳐 놓인다 (게이지는 그리지 않음)
    //  - 도면 좌표는 404 x 254 기준 (TunerGauge.cpp 의 상수), 크기가 달라도 비율대로 늘린다
    //  - No signal: 바늘/부채꼴 없음, 회색 / In tune (|cents| <= 3): 초록 (별도 글자 없음) / 그 밖: 주황
    class TunerGauge final : public juce::Component
    {
    public:
        static constexpr double inTuneCents = 3.0;
        static constexpr int panelWidth = 404, panelHeight = 254;
        static constexpr float valueFontSize = 15.0f;   // 기준음 Hz 와 오차 cents 글자

        // 색 (Live 아래 줄 튜너도 같이 쓴다)
        static inline const juce::Colour inTuneColour { 0xff32cd32 };   // 전원 버튼과 같은 초록
        static inline const juce::Colour offColour    { 0xffffa500 };   // 앱 강조색 주황
        static inline const juce::Colour idleColour   { 0xff808080 };   // juce::Colours::grey (다른 정적 값으로 초기화하면 순서 문제)

        static juce::Rectangle<int> pitchArea();   // 기준음 값 상자 자리 (도면 좌표, 음 이름 왼쪽)

        TunerGauge();

        void setState (const TunerState& newState);   // 받을 때마다 repaint (대화상자 타이머 20 Hz)
        void paint (juce::Graphics&) override;

    private:
        TunerState state;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TunerGauge)
    };
}
