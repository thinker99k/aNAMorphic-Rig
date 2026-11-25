#pragma once

#include "engine/param/common/Biquad.h"
#include "NamParams.h"
#include "engine/param/common/ParamStage.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <memory>

namespace rig
{
    // NAM 블록 뒤의 3밴드 EQ (Bass / Middle / Treble 모두 peak, 각 Freq 가 중심 주파수) + Presence / Resonance (NamParams 설명). 값은 그 블록의 지금 씬 BlockParams (번호는 NamParams), 필터 상태는 DSP 인스턴스마다 따로
    class EqStage final : public ParamStage
    {
    public:
        explicit EqStage (std::shared_ptr<BlockParams> params);

        void prepare (double sampleRate, int maxBlockSize) override;
        void process (float* inOut, int numSamples) noexcept override;

    private:
        using Linear = juce::SmoothedValue<double, juce::ValueSmoothingTypes::Linear>;
        using Multiplicative = juce::SmoothedValue<double, juce::ValueSmoothingTypes::Multiplicative>;   // 주파수는 로그로 따라간다

        void jumpToTargets() noexcept;
        void setTargets() noexcept;

        std::shared_ptr<BlockParams> params;
        Biquad low, middle, high, presence, resonanceShelf, resonancePeak;
        Linear bassDb, middleDb, trebleDb, presenceDb, resonanceDb;
        Multiplicative bassHz, middleHz, trebleHz, middleQ;   // Q 도 비율로 따라간다

        double sampleRate = 48000.0;
    };
}
