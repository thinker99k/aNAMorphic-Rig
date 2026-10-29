#pragma once

#include "Block.h"

#include <juce_dsp/juce_dsp.h>

#include <memory>

namespace rig
{
    // .wav 임펄스 응답(캐비닛 IR 등) 하나를 감싸는 블록 (JUCE dsp::Convolution 사용)
    class IrBlock final : public Block
    {
    public:
        // 파일을 읽어 블록을 만든다. 백그라운드 스레드에서 호출할 것.
        static std::unique_ptr<IrBlock> load (const juce::File& file, juce::String& error);

        BlockType type() const noexcept override { return BlockType::Ir; }
        void prepare (double sampleRate, int maxBlockSize) override;
        void process (const float* in, float* out, int numSamples) noexcept override;
        double expectedSampleRate() const noexcept override { return 0.0; } // Convolution 이 알아서 리샘플

        double irSampleRate() const noexcept { return fileSampleRate; }
        double irLengthSeconds() const noexcept { return lengthSeconds; }

    private:
        IrBlock() = default;

        juce::dsp::Convolution convolution;   // 기본값: 지연 0, 균일 파티션
        double fileSampleRate = 0.0;
        double lengthSeconds = 0.0;
    };
}
