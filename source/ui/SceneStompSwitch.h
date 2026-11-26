#pragma once

#include "model/RigModel.h"
#include "ui/Widgets.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace rig
{
    // Scene / Stomp 스위치 (편집 화면 ssbar 와 Live statbar 가 같이 쓴다, 둘 다 14칸 격자의 12번 칸)
    //  - 위 절반 가운데 "Scene", 아래 절반 가운데 "Stomp", 사이에 가로 구분선. 지금 모드(RigModel::isStompMode)는 굵게 + 색 (Scene 빨강, Stomp 파랑),
    //    다른 쪽은 흐린 보통 글자
    //  - 늘 둘 중 한 모드가 켜져 있으므로 바탕은 늘 켜짐 표시 (indicator::, 지금 모드의 색)
    //  - 왼쪽 클릭 = Scene ↔ Stomp (RigModel::setPlayMode). 마우스를 올려도 강조 없음
    class SceneStompSwitch final : public LeftClickOnly<juce::Button>,
                                   private juce::ChangeListener
    {
    public:
        explicit SceneStompSwitch (RigModel& model);
        ~SceneStompSwitch() override;

        void paintButton (juce::Graphics&, bool isMouseOver, bool isButtonDown) override;

    private:
        void changeListenerCallback (juce::ChangeBroadcaster*) override;

        RigModel& model;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SceneStompSwitch)
    };
}
