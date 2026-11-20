#pragma once

#include "model/RigModel.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "ui/Widgets.h"

namespace rig
{
    // 패치 맵 아래: 선택된 블록의 파라미터.
    //   블록  : [ (나중에 노브 등이 왼쪽부터) ...                 (전원) ]
    //   IN    : [ Input channel  [1: ...    v] ]   장치 입력 채널 하나
    //   OUT   : [ Output channel [1+2: ...  v] ]   스테레오 쌍 먼저, 그다음 단일 채널
    // 블록의 파라미터는 on 하나뿐: 오른쪽 끝 전원 버튼. 미지정 블록이나 선택된 블록이 없으면 비활성.
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

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParamBar)
    };
}
