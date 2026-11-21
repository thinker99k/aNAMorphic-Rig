#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace rig
{
    // 노브 공용 외형 (볼륨 노브, 파라미터 노브)
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
}
