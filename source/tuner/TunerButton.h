#pragma once

#include "ui/Widgets.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace rig
{
    // 상단 바의 Tuner 버튼: 다른 버튼과 같은 배경 + 왼쪽 14 x 14 아이콘(반원 호와 바늘) + "Tuner"
    // 튜너 대화상자가 열려 있는 동안(setActive (true)) 테두리와 글자가 주황
    class TunerButton final : public LeftClickOnly<juce::Button>
    {
    public:
        TunerButton();                         // 이름 "Tuner"
        void setActive (bool shouldBeActive);  // 대화상자가 열려 있는 동안 true
        void paintButton (juce::Graphics&, bool isMouseOver, bool isButtonDown) override;

        // 아이콘: 정사각형 icon 안에 반원 호 + 오른쪽으로 기운 바늘 (Live 아래 줄 튜너 스위치도 같이 쓴다)
        static void drawIcon (juce::Graphics&, juce::Rectangle<float> icon, juce::Colour, float thickness);

    private:
        bool active = false;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TunerButton)
    };
}
