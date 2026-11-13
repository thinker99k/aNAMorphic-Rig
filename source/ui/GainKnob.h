#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "ui/Widgets.h"

#include <functional>

namespace rig
{
    // 볼륨 노브 하나: 회전 노브 / 아래쪽 이름 (INPUT, OUTPUT). dB 값은 돌리는 동안만 말풍선으로 표시
    class GainKnob final : public juce::Component
    {
    public:
        // minIsInf: true 면 최솟값을 "-inf dB" 로 표시 (출력), false 면 실제 dB 로 표시 (입력)
        GainKnob (const juce::String& name, float minDb, float maxDb, bool minIsInf);
        ~GainKnob() override;

        // 사용자가 노브를 돌렸을 때만 호출 (dB)
        std::function<void (float)> onValueChanged;

        // 사용자가 드래그를 끝냈을 때. 저장은 여기서
        std::function<void()> onDragEnded;

        // 외부에서 값만 바꾼다 (onValueChanged 는 부르지 않음)
        void setValueDb (float db);

        // 아래쪽 이름 글자 표시 여부 (기본 true). 상태바처럼 옆에 이름이 있는 곳에서는 숨긴다
        void setNameVisible (bool shouldBeVisible);

        void resized() override;

    private:
        juce::String toText (double db) const;
        void parentHierarchyChanged() override;

        const float minDb;
        const bool minIsInf;
        bool nameVisible = true;

        // 노브 몸통 + 호(맨 왼쪽 ~ 현재 값) + 지시선
        struct KnobLookAndFeel final : juce::LookAndFeel_V4
        {
            void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                                   float sliderPosProportional, float rotaryStartAngle,
                                   float rotaryEndAngle, juce::Slider&) override;

            // 값 말풍선: 항상 노브 위, 꼬리 없는 둥근 사각형
            int getSliderPopupPlacement (juce::Slider&) override { return juce::BubbleComponent::above; }
            void drawBubble (juce::Graphics&, juce::BubbleComponent&, const juce::Point<float>& tip,
                             const juce::Rectangle<float>& body) override;
        };

        // slider 보다 먼저 선언: 멤버는 역순으로 소멸하므로 slider 가 먼저 사라진다
        KnobLookAndFeel knobLnf;
        LeftClickOnly<juce::Slider> slider;
        juce::Label nameLabel;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GainKnob)
    };
}
