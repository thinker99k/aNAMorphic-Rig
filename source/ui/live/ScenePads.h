#pragma once

#include "model/RigModel.h"
#include "ui/Widgets.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>

namespace rig
{
    // Live 화면의 패드 영역: 패드 8개를 4 x 2 로. Scene 모드 = 씬 8개, Stomp 모드 = Stomp 8칸
    //  - 바깥 여백 = 패드 사이 = 10 px (고정). 크기가 바뀌면 패드 크기만 변한다
    //  - Scene: 번호 왼쪽 위 + 이름 가운데 (고정 글꼴로 줄마다 채우고 넘치면 말줄임), 현재 씬은 밝게 채운다.
    //    Stomp: 묶인 패드 = 블록 모양을 키운 것 (block::drawStomp, 번호·중요 값 없음, 꺼지면 흐리게), 안 묶인 패드 = 빈 패드
    //  - 왼쪽 클릭하는 순간 그 씬으로 전환 (현재 씬이면 무시) / 묶인 블록 켜고 끄기. 오른쪽·가운데 클릭은 동작 없음 (메뉴 없음)
    //  - 마우스를 올려도 강조하지 않는다
    class ScenePads final : public juce::Component,
                            private juce::ChangeListener
    {
    public:
        explicit ScenePads (RigModel& model);
        ~ScenePads() override;

        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent&) override;

    private:
        void changeListenerCallback (juce::ChangeBroadcaster*) override;

        RigModel& model;
        std::array<juce::Rectangle<int>, RigModel::numScenes> padBounds;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScenePads)
    };
}
