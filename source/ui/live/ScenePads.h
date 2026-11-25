#pragma once

#include "model/RigModel.h"
#include "ui/Widgets.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>

namespace rig
{
    // Live 화면의 씬 패드 영역: 패드 8개를 4 x 2 로
    //  - 바깥 여백 = 패드 사이 = 10 px (고정). 크기가 바뀌면 패드 크기만 변한다
    //  - 현재 씬 패드는 밝게 채운다
    //  - 왼쪽 클릭하는 순간 그 씬으로 전환 (현재 씬이면 무시). 오른쪽·가운데 클릭은 동작 없음 (메뉴 없음)
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
