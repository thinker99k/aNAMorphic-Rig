#pragma once

// [SPECTRUM] 테스트용 보조 기능. 제거 방법:
//   1. 이 파일과 SpectrumWindow.cpp 를 지운다
//   2. 프로젝트에서 "[SPECTRUM]" 을 검색해, "begin" ~ "end" 사이 블록과 "[SPECTRUM]" 이 붙은 한 줄을 지운다
//      (AudioEngine.h/.cpp, TopBar.h/.cpp, MainComponent.h/.cpp, CMakeLists.txt)

#include "engine/AudioEngine.h"

#include <juce_dsp/juce_dsp.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <array>

namespace rig
{
    // 출력 소리의 주파수별 세기 (x: 20 Hz ~ 20 kHz 로그, y: -60 ~ 0 dBFS). 30 Hz 갱신
    class SpectrumView final : public juce::Component,
                               private juce::Timer
    {
    public:
        static constexpr int fftOrder = 13;
        static constexpr int fftSize = 1 << fftOrder;            // 8192
        static constexpr int numBins = fftSize / 2 + 1;          // 0 ~ 나이퀴스트
        static constexpr float minHz = 20.0f, maxHz = 20000.0f;
        static constexpr float floorDb = -60.0f, topDb = 0.0f;
        static constexpr float fallDbPerFrame = 1.0f;            // 내려갈 때만 천천히 (30 Hz 에서 약 30 dB/s)
        static constexpr int refreshHz = 30;

        explicit SpectrumView (AudioEngine& engine);
        ~SpectrumView() override;

        // 보일 때만 켠다: 꺼지면 엔진 기록과 화면 갱신을 모두 멈춘다
        void setRunning (bool shouldRun);

        void visibilityChanged() override;
        void paint (juce::Graphics&) override;

    private:
        void timerCallback() override;
        float dbAtBin (float bin) const;   // 이웃한 두 빈 사이 선형 보간

        AudioEngine& engine;
        juce::dsp::FFT fft { fftOrder };
        std::array<float, fftSize> input {}, window {};
        std::array<float, fftSize * 2> work {};                  // FFT 는 2N 크기 배열을 요구한다
        std::array<float, numBins> shown {};                     // 표시용 dB

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumView)
    };

    // 모달이 아닌 스펙트럼 창. 닫기 버튼은 창을 지우지 않고 숨기기만 한다 (MainComponent 가 소유)
    class SpectrumWindow final : public juce::DocumentWindow
    {
    public:
        explicit SpectrumWindow (AudioEngine& engine);
        ~SpectrumWindow() override;

        void closeButtonPressed() override;
        void visibilityChanged() override;

    private:
        SpectrumView* view = nullptr;   // setContentOwned 로 창이 소유

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumWindow)
    };
}
