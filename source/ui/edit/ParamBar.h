#pragma once

#include "model/RigModel.h"
#include "ParamPanel.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include "ui/Widgets.h"

#include <memory>

namespace rig
{
    // 패치 맵 아래: 선택된 블록의 파라미터.
    //   [ 블록 종류별 파라미터 패널 (ParamPanel) ...         (전원) ]
    // 패널은 선택된 블록의 종류가 바뀔 때 createParamPanel 로 다시 만든다 (파라미터가 없는 종류는 패널 없음).
    // 오른쪽 끝 전원 버튼: 미지정 블록이나 선택된 블록이 없으면 비활성
    class ParamBar final : public juce::Component,
                           private juce::ChangeListener
    {
    public:
        explicit ParamBar (RigModel& model);
        ~ParamBar() override;

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        // 전원 아이콘 토글 버튼: 켜짐 = 초록, 꺼짐 = 흐린 회색
        struct PowerButton final : public LeftClickOnly<juce::Button>
        {
            PowerButton() : LeftClickOnly ("Power") {}
            void paintButton (juce::Graphics&, bool highlighted, bool down) override;
        };

        void changeListenerCallback (juce::ChangeBroadcaster*) override;
        void updateFromModel();

        RigModel& model;
        PowerButton powerButton;
        std::unique_ptr<ParamPanel> panel;
        BlockType panelType = BlockType::None;   // 지금 패널의 블록 종류

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParamBar)
    };
}
