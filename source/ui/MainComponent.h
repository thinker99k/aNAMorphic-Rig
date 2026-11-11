#pragma once

#include "engine/AudioEngine.h"
#include "model/RigModel.h"
#include "ui/BlockEditor.h"
#include "ui/ChainView.h"
#include "ui/StatusBar.h"
#include "ui/TopBar.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace rig
{
    // 화면 배치
    //  ┌──────────────────────────────────────────────┐
    //  │ TopBar   : Patch [콤보] [◀][▶]  [패치명]  [Settings] │
    //  ├──────────────────────────────────────────────┤
    //  │ ChainView (본문 상단 절반)                     │
    //  ├──────────────────────────────────────────────┤
    //  │ BlockEditor (본문 하단 절반 전체)        │
    //  ├──────────────────────────────────────────────┤
    //  │ StatusBar (62): [노브] IN | CPU | [MUTE] OUT [노브] │
    //  └──────────────────────────────────────────────┘
    class MainComponent final : public juce::Component,
                                private juce::Timer
    {
    public:
        MainComponent (juce::AudioDeviceManager& deviceManager, AudioEngine& engine, RigModel& model);

        // 창 크기 (고정)
        static constexpr int minWidth  = 1024;
        static constexpr int minHeight = 768;
        
        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        void timerCallback() override;
        void openSettings();
        void updateMuteButton();   // 뮤트 상태(RigModel 소유)를 버튼 색으로 보여준다

        juce::AudioDeviceManager& deviceManager;
        AudioEngine& engine;
        RigModel& model;

        TopBar topBar;
        ChainView chainView;
        BlockEditor blockEditor;
        StatusBar statusBar;   // 볼륨 노브와 MUTE 버튼도 여기 있다

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
    };
}
