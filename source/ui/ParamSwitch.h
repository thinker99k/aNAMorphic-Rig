#pragma once

#include "ui/Widgets.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace rig
{
    // 파라미터 토글: 왼쪽에 글자(EQ, LC, RC), 오른쪽에 알약 모양 스위치. 누르면 켜짐/꺼짐이 바뀐다 (왼쪽 클릭만).
    // 켜짐 = 초록(ParamBar 전원 버튼과 같은 색), 꺼짐 = 흐린 회색. 호버·눌림 색 변화도 전원 버튼과 같은 방식
    struct ParamSwitch final : public LeftClickOnly<juce::Button>
    {
        explicit ParamSwitch (const juce::String& text);
        void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    };
}
