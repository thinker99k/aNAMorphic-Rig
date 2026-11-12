#pragma once

#include "model/RigModel.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace rig
{
    // 패치 맵 아래: 선택된 블록의 파라미터.
    //   [ (나중에 노브 등이 왼쪽부터) ...                 (전원) ]
    // 지금 파라미터는 on 하나뿐: 오른쪽 끝 전원 버튼. 미지정 블록이나 선택된 블록이 없으면 비활성
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
        struct PowerButton final : public juce::Button
        {
            PowerButton() : juce::Button ("Power") {}
            void paintButton (juce::Graphics&, bool highlighted, bool down) override;
        };

        void changeListenerCallback (juce::ChangeBroadcaster*) override;
        void updateFromModel();

        RigModel& model;
        PowerButton powerButton;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParamBar)
    };
}
