#pragma once

#include "Block.h"

#include <memory>
#include <vector>

namespace nam { class DSP; }

namespace rig
{
    class BlockParams;

    // .nam 캡처 하나를 감싸는 블록 (NeuralAmpModelerCore 사용). params 가 있으면 모델 앞에 Gain (입력 게인, 드라이브)
    class NamBlock final : public Block
    {
    public:
        // 파일을 읽어 모델을 만든다. 무거운 작업이므로 백그라운드 스레드에서 호출할 것.
        // 실패하면 nullptr 을 돌려주고 error 에 이유를 채운다.
        // params 가 있으면 모델 앞에 Gain, 블록 소리 뒤에 EQ 를 건다
        static std::unique_ptr<NamBlock> load (const juce::File& file, juce::String& error,
                                               std::shared_ptr<BlockParams> params = nullptr);

        ~NamBlock() override;

        BlockType type() const noexcept override { return BlockType::Nam; }
        void prepare (double sampleRate, int maxBlockSize) override;
        void process (const float* in, float* out, int numSamples) noexcept override;
        double expectedSampleRate() const noexcept override;

    private:
        explicit NamBlock (std::unique_ptr<nam::DSP> model);
        std::unique_ptr<nam::DSP> model;
        std::unique_ptr<ParamStage> inputGain;   // 모델 앞 Gain (params 가 없으면 nullptr)
        std::vector<float> gained;               // 게인을 곱한 입력 (in 은 dry 로도 쓰이므로 건드리지 않는다). prepare 에서 할당
    };
}
