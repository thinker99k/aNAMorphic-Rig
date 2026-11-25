#pragma once

#include "BlockParams.h"
#include "ParamStage.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <memory>

namespace rig
{
    // dB 파라미터 하나를 게인으로 곱하는 단계 (공용 Output). 값 변화는 진폭 선형 20 ms (같은 소리의 두 버전)
    class GainStage final : public ParamStage
    {
    public:
        GainStage (std::shared_ptr<BlockParams> p, int index) : params (std::move (p)), dbIndex (index) {}

        void prepare (double sampleRate, int) override
        {
            gain.reset (sampleRate, transitionSeconds);
            gain.setCurrentAndTargetValue (target());   // 오디오 스레드가 쓰기 전이므로 바로 맞춘다
        }

        void process (float* io, int n) noexcept override
        {
            gain.setTargetValue (target());
            gain.applyGain (io, n);
        }

    private:
        float target() const noexcept { return juce::Decibels::decibelsToGain (params->get (dbIndex)); }

        std::shared_ptr<BlockParams> params;
        const int dbIndex;
        juce::SmoothedValue<float> gain { 1.0f };
    };
}
