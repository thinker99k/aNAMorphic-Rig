#pragma once

#include "engine/param/common/Biquad.h"
#include "IrParams.h"
#include "engine/param/common/ParamStage.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <memory>

namespace rig
{
    // IR 블록 뒤의 LC(high-pass) / RC(low-pass) 컷. 각각 Butterworth cutOrder 차 = 2차 필터 cutOrder/2 단 직렬.
    // 값은 그 블록의 IrParams, 필터 상태는 DSP 인스턴스마다 따로. LC 와 RC 는 각자 스위치와 혼합 비율을 가진다
    class CutStage final : public ParamStage
    {
    public:
        explicit CutStage (std::shared_ptr<IrParams> params);

        void prepare (double sampleRate, int maxBlockSize) override;
        void process (float* inOut, int numSamples) noexcept override;

    private:
        static constexpr int numStages = IrParams::cutOrder / 2;

        struct Section
        {
            bool highPass = false;   // LC = high-pass, RC = low-pass
            std::array<Biquad, numStages> stages;
            juce::SmoothedValue<double, juce::ValueSmoothingTypes::Multiplicative> hz;   // 주파수는 로그로 따라간다
            double mix = 0.0;        // 0 = 건너뜀, 1 = 필터 소리
        };

        void processSection (Section&, bool on, double targetHz, float* io, int n) noexcept;

        std::shared_ptr<IrParams> params;
        Section lowCut, highCut;
        std::array<double, numStages> q {};   // Butterworth 단별 Q

        double sampleRate = 48000.0;
        double mixStep = 1.0;
    };
}
