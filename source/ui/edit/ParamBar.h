#pragma once

#include "model/RigModel.h"
#include "ParamPanel.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "ui/ParamKnob.h"
#include "ui/Widgets.h"

#include <array>
#include <memory>

namespace rig
{
    // 패치 맵 아래: 선택된 블록 / 합류 / IN·OUT 의 파라미터.
    //   블록  : [ [Param Reset] 블록 종류별 파라미터 패널 (ParamPanel) ...   | (Output) (전원) ]   현재 씬의 값. Output 은 모든 종류 공용
    //   합류  : [ [Level Reset] (빨간 전원) Upper  -3.0 dB   (빨간 전원) Lower  -3.0 dB        ]   현재 씬의 값
    //                   ━━━━━━●━━               ━━━━━━●━━
    //   IN    : [ Input channel  [1: ...    v] ]   장치 입력 채널 하나
    //   OUT   : [ Output channel [1+2: ...  v] ]   스테레오 쌍 먼저, 그다음 단일 채널
    // 블록의 파라미터는 on 하나뿐: 오른쪽 끝 전원 버튼. 미지정 블록이나 선택된 블록이 없으면 비활성.
    // 오류(빨강)·샘플레이트 경고(노랑) 문구는 패치 맵 왼쪽 위 (ChainView).
    // 합류 레벨 슬라이더: -inf(-60) ~ 0 dB, 오디오 테이퍼(위치 절반마다 -12 dB), 상대 드래그(클릭한 곳으로 뛰지 않음),
    // 더블클릭 = -3 dB, 저장은 드래그 끝에.
    // 슬라이더 위 글자(왼쪽 = 갈래 이름 굵게, 오른쪽 = dB)의 양 끝은 홈의 양 끝에 맞춘다.
    // 패널은 선택된 블록의 종류가 바뀔 때 createParamPanel 로 다시 만든다 (파라미터가 없는 종류는 패널 없음).
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
        void changeListenerCallback (juce::ChangeBroadcaster*) override;
        void updateFromModel();
        void updateMerge (const RigModel::Parallel&);
        void fillChannelBox (RigModel::Endpoint);
        void channelChosen();

        RigModel& model;
        juce::AudioDeviceManager& deviceManager;   // 채널 이름과 개수 (장치가 바뀌면 목록을 다시 만든다)
        PowerButton powerButton;
        juce::Label channelLabel;
        LeftClickOnly<juce::ComboBox> channelBox;

        // 합류 레벨 슬라이더 모양: 콘솔 페이더 (가는 어두운 홈 + 세로로 긴 사각 손잡이, 가운데 흰 선)
        struct FaderLookAndFeel final : juce::LookAndFeel_V4
        {
            static constexpr int capWidth = 12, capHeight = 24;

            // 손잡이 폭의 절반만큼 홈 양 끝을 들인다 (값 0 / 1 에서 손잡이 가운데 = 홈 끝)
            int getSliderThumbRadius (juce::Slider&) override { return capWidth / 2; }

            void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos, float minSliderPos,
                                   float maxSliderPos, juce::Slider::SliderStyle, juce::Slider&) override;
        };

        // 합류: [0] = 위 갈래, [1] = 아래 갈래. faderLnf 는 슬라이더보다 먼저 선언 (멤버는 역순으로 소멸)
        FaderLookAndFeel faderLnf;
        std::array<SubPowerButton, 2> branchPower;
        std::array<juce::Label, 2> branchLabel, levelValue;
        std::array<LeftClickOnly<juce::Slider>, 2> levelSlider;
        LeftClickOnly<juce::TextButton> levelResetButton { "Level\nReset" };
        LeftClickOnly<juce::TextButton> paramResetButton { "Param\nReset" };   // 블록 파라미터가 있는 종류만, 왼쪽 끝 (Level Reset 과 같은 자리)
        static constexpr int outputCell = 12;   // 공용 Output 칸 (모든 블록 종류 고정, 앞에 구분선 |)
        ParamKnob outputKnob { "Output", CommonParams::outputMinDb, CommonParams::outputMaxDb, CommonParams::outputDefaultDb, ParamKnob::ValueType::Db };
        std::unique_ptr<ParamPanel> panel;
        BlockType panelType = BlockType::None;   // 지금 패널의 블록 종류

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParamBar)
    };
}
