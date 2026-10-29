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
    //  │ TopBar   : Patch [콤보]            [Settings] │
    //  ├──────────────────────────────────────────────┤
    //  │ ChainView (본문 상단 절반)                     │
    //  ├──────────────────────────────────────────────┤
    //  │ BlockEditor (본문 하단 절반의 위쪽 절반)        │
    //  ├──────────────────────────────────────────────┤
    //  │ (예약 영역: 이후 기능용)                       │
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

        juce::AudioDeviceManager& deviceManager;
        AudioEngine& engine;
        RigModel& model;

        TopBar topBar;
        ChainView chainView;
        BlockEditor blockEditor;
        StatusBar statusBar;

        juce::Rectangle<int> reservedArea;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
    };
}
