#pragma once

#include "tuner/PitchDetector.h"
#include "tuner/TunerGauge.h"
#include "tuner/TunerInput.h"
#include "ui/Widgets.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>
#include <vector>

namespace rig
{
    // 튜너 모달 대화상자 내용물 (420 x 318, 제목 표시줄 22 를 더해 창 420 x 340)
    //  - [게이지 패널 404 x 254] 아래 [A4 (-) 440.0 Hz (+)    ● Source: IN 1]
    //  - 열려 있는 동안만 입력 콜백(TunerInput)을 등록하고 20 Hz 로 분석한다
    //  - 오른쪽 위 X 로만 닫는다 (ESC, 키보드로는 닫히지 않는다). 바뀐 기준음은 닫을 때 저장
    class TunerDialog final : public juce::Component,
                              private juce::Timer
    {
    public:
        static constexpr bool mutesOutput = true;   // 튜너가 열려 있는 동안 출력을 끈다 (3-3절). 소리를 그대로 내보내려면 false

        // 모달 창으로 띄운다. inputChannel = 지금 패치의 입력 채널 (0부터, Routing::input). 닫힐 때(X) onClosed 호출
        static void show (juce::AudioDeviceManager& deviceManager, int inputChannel,
                          juce::Component* parent, std::function<void()> onClosed);

        TunerDialog (juce::AudioDeviceManager& deviceManager, int inputChannel);
        ~TunerDialog() override;

        void close();   // 창의 X 버튼이 부른다

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        void timerCallback() override;
        void stepPitch (int direction);
        void updatePitchControls();

        juce::AudioDeviceManager& deviceManager;
        std::function<void()> onClosed;
        bool closed = false;

        TunerInput input;
        PitchDetector detector;
        std::vector<float> samples;
        double preparedRate = 0.0;

        static constexpr int medianCount = 5;   // 중앙값에 쓰는 최근 값 개수
        std::array<double, medianCount> recent {};
        int recentCount = 0;
        int failedFrames = 0;
        TunerState state;

        int pitchTenths, loadedTenths;   // A4 기준음 (0.1 Hz 단위)

        TunerGauge gauge;
        juce::Label a4Label { {}, "A4" };
        LeftClickOnly<juce::TextButton> downButton { juce::String::fromUTF8 ("\xe2\x88\x92") };   // −
        juce::Label pitchLabel;
        LeftClickOnly<juce::TextButton> upButton { "+" };
        juce::Label sourceLabel;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TunerDialog)
    };
}
