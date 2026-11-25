#pragma once

#include "engine/AudioEngine.h"
#include "model/RigModel.h"
#include "tuner/TunerAnalyzer.h"
#include "tuner/TunerStrip.h"
#include "ui/SceneStompSwitch.h"
#include "ui/Widgets.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace rig
{
    // Live 화면의 아래 줄 (편집 화면 씬 바 자리). 씬 바와 같은 14칸 격자 (bottomBarCell): 6 : 빈칸 1 : 1 : 2 : 빈칸 1 : 1 : 빈칸 1 : 1
    //   [Input channel  [목록]]  [튜너 스위치] [간이 튜너] [ ] [Scene / Stomp] [ ] [Live]
    //   [Output channel [목록]]
    //  - 채널: Live 전용 (RigModel::setLiveRouting). 패치 파일·Undo 와 무관하고 저장하지 않는다.
    //          목록 모양은 파라미터 바의 IN / OUT 과 같다 (fillChannelList). Live 에는 다른 곳이 쓰는 채널이 없어 막는 항목 없음
    //  - 튜너 스위치: 버튼 자체가 켜짐 표시 (켜짐 = 주황 테두리, 글자 굵게). 켜져 있는 동안 출력 무음 (AudioEngine::setTunerActive,
    //                 MUTE 와 별개), 간이 튜너(TunerStrip)를 20 Hz 로 갱신. 기준음은 설정의 값 (튜너 모달에서 마지막으로 쓴 값).
    //                 꺼져 있으면 간이 튜너 칸 전체를 흐리게 (비활성)
    //  - Scene / Stomp: 편집 화면 씬 바와 같은 스위치 (SceneStompSwitch, 같은 11번 칸)
    //  - Live: 편집 화면으로 (켜짐 표시, 오른쪽 끝)
    //  - 왼쪽 클릭만 받는다
    class LiveStatusBar final : public juce::Component,
                                private juce::ChangeListener,
                                private juce::Timer
    {
    public:
        LiveStatusBar (RigModel& model, AudioEngine& engine, juce::AudioDeviceManager& deviceManager);
        ~LiveStatusBar() override;

        std::function<void()> onLiveOff;   // Live 버튼

        void setTunerOn (bool shouldBeOn);   // Live 에 들어갈 때·나갈 때 끈다

        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent&) override;

    private:
        void changeListenerCallback (juce::ChangeBroadcaster*) override;
        void timerCallback() override;
        void updateChannels();
        AudioEngine::Routing liveRouting() const { return model.getLiveRouting().value_or (model.getRouting()); }

        RigModel& model;
        AudioEngine& engine;
        juce::AudioDeviceManager& deviceManager;   // 채널 이름과 개수

        juce::Label inputLabel { {}, "Input channel" }, outputLabel { {}, "Output channel" };
        LeftClickOnly<juce::ComboBox> inputBox, outputBox;

        bool tunerOn = false;
        TunerAnalyzer analyzer;
        TunerStrip tunerStrip;
        SceneStompSwitch modeSwitch;

        juce::Rectangle<int> tunerBounds, liveBounds;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LiveStatusBar)
    };
}
