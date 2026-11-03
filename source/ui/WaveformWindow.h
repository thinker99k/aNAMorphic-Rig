#pragma once

#include "engine/AudioEngine.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <vector>

namespace rig
{
    // 입력(왼쪽) / 출력(오른쪽) 파형을 나란히 그리는 오실로스코프. 각각 최근 약 50 ms
    class WaveformView final : public juce::Component,
                               private juce::Timer
    {
    public:
        explicit WaveformView (AudioEngine& engine);
        ~WaveformView() override;

        // 창이 보일 때만 켠다: 꺼지면 엔진 기록과 화면 갱신을 모두 멈춘다
        void setRunning (bool shouldRun);

        void paint (juce::Graphics&) override;

    private:
        void timerCallback() override;

        // 최근 2N 개를 가져와 상승 영점 통과 지점부터 N 개를 dest 에 담는다. 실패하면 dest 는 그대로
        void capture (bool input, int numSamples, std::vector<float>& dest);

        static void drawPanel (juce::Graphics&, juce::Rectangle<int> bounds,
                               const juce::String& title, const std::vector<float>& samples);

        AudioEngine& engine;
        std::vector<float> fetchBuffer;                  // copyLatest 용 작업 버퍼
        std::vector<float> inputSamples, outputSamples;  // 마지막으로 성공한 프레임의 표시 구간

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformView)
    };

    // 모달이 아닌 파형 창. 닫기 버튼은 창을 지우지 않고 숨기기만 한다 (MainComponent 가 소유)
    class WaveformWindow final : public juce::DocumentWindow
    {
    public:
        explicit WaveformWindow (AudioEngine& engine);
        ~WaveformWindow() override;

        void closeButtonPressed() override;
        void visibilityChanged() override;

    private:
        WaveformView* view = nullptr;   // setContentOwned 로 창이 소유

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformWindow)
    };
}
