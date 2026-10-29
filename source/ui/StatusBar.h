#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace rig
{
    // 가로 막대 게이지 하나 (레벨 미터 / CPU 게이지 공용)
    class Gauge final : public juce::Component
    {
    public:
        Gauge (juce::String caption, juce::Colour colour);

        // fraction: 0..1 (막대 길이), text: 오른쪽에 보일 수치
        void setValue (float fraction, const juce::String& text);

        void paint (juce::Graphics&) override;

    private:
        juce::String caption, valueText;
        juce::Colour colour;
        float fraction = 0.0f;
    };

    // 최하단 상태 바: [Input 미터](왼쪽) ...... [CPU](가운데) ...... [Output 미터](오른쪽)
    class StatusBar final : public juce::Component
    {
    public:
        StatusBar();

        // 피크 값(선형 0..1+), CPU(0..1) 를 받아 갱신. UI 타이머에서 호출.
        void update (float inputPeak, double cpuUsage, float outputPeak);

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        static float toMeterFraction (float dB);

        Gauge input  { "IN",  juce::Colours::limegreen };
        Gauge cpu    { "CPU", juce::Colours::orange };
        Gauge output { "OUT", juce::Colours::limegreen };

        float inputDb = -100.0f, outputDb = -100.0f;   // 감쇠(decay) 를 위한 표시값

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StatusBar)
    };
}
