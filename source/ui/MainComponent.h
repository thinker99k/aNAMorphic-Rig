#pragma once

#include "engine/AudioEngine.h"
#include "model/RigModel.h"
#include "ui/BlockEditor.h"
#include "ui/ChainView.h"
#include "ui/GainKnob.h"
#include "ui/StatusBar.h"
#include "ui/TopBar.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace rig
{
    // 화면 배치
    //  ┌──────────────────────────────────────────────┐
    //  │ TopBar   : Patch [콤보]            [Settings] │
    //  ├──────────────────────────────────────────────┤
    //  │ ChainView (본문 상단 절반)                     │
    //  ├──────────────────────────────────────────────┤
    //  │ BlockEditor (본문 하단 절반의 위쪽 절반)        │
    //  ├──────────────────────────────────────────────┤
    //  │ [INPUT 노브]   (예약 영역)  [OUTPUT 노브][MUTE] │
    //  ├──────────────────────────────────────────────┤
    //  │ StatusBar : IN 미터 | CPU | OUT 미터            │
    //  └──────────────────────────────────────────────┘
    class MainComponent final : public juce::Component,
                                private juce::Timer
    {
    public:
        MainComponent (juce::AudioDeviceManager& deviceManager, AudioEngine& engine, RigModel& model);

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
        StatusBar statusBar;
        GainKnob inputKnob  { "INPUT",  AudioEngine::inputMinDb,  AudioEngine::inputMaxDb,  false };
        GainKnob outputKnob { "OUTPUT", AudioEngine::outputMinDb, AudioEngine::outputMaxDb, true  };
        juce::TextButton muteButton { "MUTE" };

        juce::Rectangle<int> reservedArea;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
    };
}
