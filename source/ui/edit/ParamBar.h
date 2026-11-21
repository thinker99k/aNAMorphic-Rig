#pragma once

#include "model/RigModel.h"
#include "ParamPanel.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "ui/Widgets.h"

#include <memory>

namespace rig
{
    // 패치 맵 아래: 선택된 블록의 파라미터.
    //   블록  : [ 블록 종류별 파라미터 패널 (ParamPanel) ...      (전원) ]
    //   IN    : [ Input channel  [1: ...    v] ]   장치 입력 채널 하나
    //   OUT   : [ Output channel [1+2: ...  v] ]   스테레오 쌍 먼저, 그다음 단일 채널
    // 패널은 선택된 블록의 종류가 바뀔 때 createParamPanel 로 다시 만든다 (파라미터가 없는 종류는 패널 없음).
    // 오른쪽 끝 전원 버튼: 미지정 블록이나 선택된 블록이 없으면 비활성. IN / OUT 을 고르면 패널과 전원 버튼은 숨긴다.
    // 채널 목록에서 패치의 다른 곳이 쓰는 채널은 고를 수 없다 (RigModel::getUsedInputChannels / OutputChannels)
    class ParamBar final : public juce::Component,
                           private juce::ChangeListener
    {
    public:
        ParamBar (RigModel& model, juce::AudioDeviceManager& deviceManager);
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
        void fillChannelBox (RigModel::Endpoint);
        void channelChosen();

        RigModel& model;
        juce::AudioDeviceManager& deviceManager;   // 채널 이름과 개수 (장치가 바뀌면 목록을 다시 만든다)
        PowerButton powerButton;
        juce::Label channelLabel;
        LeftClickOnly<juce::ComboBox> channelBox;
        std::unique_ptr<ParamPanel> panel;
        BlockType panelType = BlockType::None;   // 지금 패널의 블록 종류

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParamBar)
    };
}
