#pragma once

#include "engine/param/common/Biquad.h"
#include "NamParams.h"
#include "engine/param/common/ParamStage.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <memory>

namespace rig
{
    // NAM 블록 뒤의 3밴드 EQ (low shelf -> peak -> high shelf). 값은 패치의 NamParams 를 공유, 필터 상태는 블록마다 따로
    class EqStage final : public ParamStage
    {
    public:
        explicit EqStage (std::shared_ptr<NamParams> params);

        void prepare (double sampleRate, int maxBlockSize) override;
        void process (float* inOut, int numSamples) noexcept override;

    private:
        using Linear = juce::SmoothedValue<double, juce::ValueSmoothingTypes::Linear>;
        using Multiplicative = juce::SmoothedValue<double, juce::ValueSmoothingTypes::Multiplicative>;   // 주파수는 로그로 따라간다

        void jumpToTargets() noexcept;
        void setTargets() noexcept;

        std::shared_ptr<NamParams> params;
        Biquad low, middle, high;
        Linear bassDb, middleDb, trebleDb;
        Multiplicative bassHz, middleHz, trebleHz;

        double sampleRate = 48000.0;
        double mix = 0.0;       // 0 = 필터 건너뜀, 1 = 필터 소리. 스위치를 켜고 끌 때 rampSeconds 동안 선형으로 넘어간다
        double mixStep = 1.0;
    };
}
