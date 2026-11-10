#pragma once

#include "engine/AudioEngine.h"
#include "ui/GainKnob.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace rig
{
    // 가로 막대 게이지 하나 (레벨 미터 / CPU 게이지 공용)
    class Gauge final : public juce::Component
    {
    public:
        Gauge (juce::String caption, juce::Colour colour);

        // fraction: 0..1 (막대 길이), text: 이름 줄 오른쪽 끝에 보일 수치 (비우면 표시 안 함)
        void setValue (float fraction, const juce::String& text = {});

        void paint (juce::Graphics&) override;

    private:
        juce::String caption, valueText;
        juce::Colour colour;
        float fraction = 0.0f;
    };

    class StatusBar final : public juce::Component
    {
    public:
        StatusBar();

        // 콜백 연결은 MainComponent 가 한다
        GainKnob inputKnob  { "INPUT",  AudioEngine::inputMinDb,  AudioEngine::inputMaxDb,  false };
        GainKnob outputKnob { "OUTPUT", AudioEngine::outputMinDb, AudioEngine::outputMaxDb, true  };
        juce::TextButton muteButton { "MUTE" };

        // 피크 값(선형 0..1+), CPU(0..1) 를 받아 갱신. UI 타이머에서 호출.
        void update (float inputPeak, double cpuUsage, float outputPeak);

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        static float toMeterFraction (float dB);

        Gauge input  { "IN",  juce::Colours::limegreen };
        Gauge cpu    { "CPU", juce::Colours::orange };
        Gauge output { "OUT", juce::Colours::limegreen };

        float inputDb = -100.0f, outputDb = -100.0f;   // 감쇠(decay) 를 위한 표시값

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StatusBar)
    };
}
