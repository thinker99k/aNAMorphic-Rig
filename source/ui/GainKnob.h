#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace rig
{
    // 볼륨 노브 하나: 위쪽 dB 값 / 회전 노브 / 아래쪽 이름 (INPUT, OUTPUT)
    class GainKnob final : public juce::Component
    {
    public:
        // minIsInf: true 면 최솟값을 "-inf dB" 로 표시 (출력), false 면 실제 dB 로 표시 (입력)
        GainKnob (const juce::String& name, float minDb, float maxDb, bool minIsInf);
        ~GainKnob() override;

        // 사용자가 노브를 돌렸을 때만 호출 (dB)
        std::function<void (float)> onValueChanged;

        // 외부에서 값만 바꾼다 (onValueChanged 는 부르지 않음)
        void setValueDb (float db);

        void resized() override;

    private:
        juce::String toText (double db) const;
        void updateValueLabel();

        const float minDb;
        const bool minIsInf;

        // 노브 몸통 + 호(맨 왼쪽 ~ 현재 값) + 지시선
        struct KnobLookAndFeel final : juce::LookAndFeel_V4
        {
            void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                                   float sliderPosProportional, float rotaryStartAngle,
                                   float rotaryEndAngle, juce::Slider&) override;
        };

        // slider 보다 먼저 선언: 멤버는 역순으로 소멸하므로 slider 가 먼저 사라진다
        KnobLookAndFeel knobLnf;
        juce::Slider slider;
        juce::Label valueLabel, nameLabel;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GainKnob)
    };
}
