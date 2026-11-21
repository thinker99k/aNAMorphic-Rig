#pragma once

#include "model/RigModel.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "ui/Widgets.h"

#include <array>

namespace rig
{
    // 패치 맵 아래: 선택된 블록 / 합류 / IN·OUT 의 파라미터.
    //   블록  : [ (나중에 노브 등이 왼쪽부터) ...      (오류 / 경고)  (전원) ]
    //   합류  : [ (전원) Upper ━━●━ -3.0 dB   (전원) Lower ━━●━ -3.0 dB          [Level Reset] ]   현재 씬의 값
    //   IN    : [ Input channel  [1: ...    v]   (오류) ]   장치 입력 채널 하나
    //   OUT   : [ Output channel [1+2: ...  v]   (오류) ]   스테레오 쌍 먼저, 그다음 단일 채널
    // 블록의 파라미터는 on 하나뿐: 오른쪽 끝 전원 버튼. 미지정 블록이나 선택된 블록이 없으면 비활성.
    // 오류(빨강) = 블록 로딩 실패, 장치에 없는 채널. 경고(노랑) = NAM 모델 샘플레이트와 장치 샘플레이트가 다름.
    // 합류 레벨 슬라이더: -inf(-60) ~ 0 dB, 오디오 테이퍼(위치 절반마다 -10 dB), 더블클릭 = -3 dB, 저장은 드래그 끝에.
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
        void updateMerge (const RigModel::Parallel&);
        void setMessages (const juce::String& error, const juce::String& warning);
        void fillChannelBox (RigModel::Endpoint);
        void channelChosen();

        RigModel& model;
        juce::AudioDeviceManager& deviceManager;   // 채널 이름과 개수 (장치가 바뀌면 목록을 다시 만든다)
        PowerButton powerButton;
        juce::Label channelLabel;
        LeftClickOnly<juce::ComboBox> channelBox;
        juce::Label errorLabel, warningLabel;

        // 합류: [0] = 위 갈래, [1] = 아래 갈래
        std::array<PowerButton, 2> branchPower;
        std::array<juce::Label, 2> branchLabel, levelValue;
        std::array<LeftClickOnly<juce::Slider>, 2> levelSlider;
        LeftClickOnly<juce::TextButton> levelResetButton { "Level Reset" };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParamBar)
    };
}
