#pragma once

#include "Block.h"

#include <memory>

namespace nam { class DSP; }

namespace rig
{
    // .nam 캡처 하나를 감싸는 블록 (NeuralAmpModelerCore 사용)
    class NamBlock final : public Block
    {
    public:
        // 파일을 읽어 모델을 만든다. 무거운 작업이므로 백그라운드 스레드에서 호출할 것.
        // 실패하면 nullptr 을 돌려주고 error 에 이유를 채운다.
        static std::unique_ptr<NamBlock> load (const juce::File& file, juce::String& error);

        ~NamBlock() override;

        BlockType type() const noexcept override { return BlockType::Nam; }
        void prepare (double sampleRate, int maxBlockSize) override;
        void process (const float* in, float* out, int numSamples) noexcept override;
        double expectedSampleRate() const noexcept override;

    private:
        explicit NamBlock (std::unique_ptr<nam::DSP> model);
        std::unique_ptr<nam::DSP> model;
    };
}
