#include "NamBlock.h"
#include "AppPaths.h"

#include <NAM/dsp.h>
#include <NAM/get_dsp.h>

namespace rig
{
    std::unique_ptr<NamBlock> NamBlock::load (const juce::File& file, juce::String& error)
    {
        if (! file.existsAsFile())
        {
            error = "File not found";
            return nullptr;
        }

        try
        {
            // prewarm 은 prepare() 의 Reset() 에서 하므로 로딩 단계에서는 생략
            nam::DspLoadOptions options;
            options.prewarm = false;

            auto model = nam::get_dsp (paths::toStdPath (file), options);

            if (model == nullptr)
            {
                error = "Could not create model";
                return nullptr;
            }

            // 샘플 앱에서는 모노 입력 / 모노 출력 모델만 지원
            if (model->NumInputChannels() != 1 || model->NumOutputChannels() != 1)
            {
                error = "Only mono-in / mono-out models are supported for now ("
                      + juce::String (model->NumInputChannels()) + " in, "
                      + juce::String (model->NumOutputChannels()) + " out)";
                return nullptr;
            }

            return std::unique_ptr<NamBlock> (new NamBlock (std::move (model)));
        }
        catch (const std::exception& e)
        {
            error = e.what();
        }
        catch (...)
        {
            error = "Unknown error while loading model";
        }

        return nullptr;
    }

    NamBlock::NamBlock (std::unique_ptr<nam::DSP> m) : model (std::move (m)) {}
    NamBlock::~NamBlock() = default;

    void NamBlock::prepare (double sampleRate, int maxBlockSize)
    {
        // Reset() 은 메모리를 할당하고 prewarm 까지 수행 -> 오디오 스레드에서 호출 금지
        model->Reset (sampleRate, maxBlockSize);
        rememberPrepare (sampleRate, maxBlockSize);
    }

    void NamBlock::process (const float* in, float* out, int numSamples) noexcept
    {
        // NAM API 는 [채널][샘플] 형태의 포인터 배열을 받는다
        float* inputs[1]  = { const_cast<float*> (in) };
        float* outputs[1] = { out };
        model->process (inputs, outputs, numSamples);
    }

    double NamBlock::expectedSampleRate() const noexcept
    {
        const auto sr = model->GetExpectedSampleRate();
        return sr > 0.0 ? sr : 0.0;
    }
}
