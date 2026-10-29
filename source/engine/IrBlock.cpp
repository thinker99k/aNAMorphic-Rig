#include "IrBlock.h"

#include <juce_audio_formats/juce_audio_formats.h>

namespace rig
{
    std::unique_ptr<IrBlock> IrBlock::load (const juce::File& file, juce::String& error)
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();

        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

        if (reader == nullptr)
        {
            error = "Not a readable audio file";
            return nullptr;
        }

        const auto numSamples = (int) reader->lengthInSamples;

        if (numSamples <= 0)
        {
            error = "Empty audio file";
            return nullptr;
        }

        // 모노 체인이므로 첫 번째 채널만 사용
        juce::AudioBuffer<float> ir (1, numSamples);
        reader->read (&ir, 0, numSamples, 0, true, false);

        auto block = std::unique_ptr<IrBlock> (new IrBlock());
        block->fileSampleRate = reader->sampleRate;
        block->lengthSeconds = numSamples / reader->sampleRate;

        // 버퍼 소유권을 넘기는 버전: 오디오 스레드에서 할당 없이 IR 이 교체됨.
        // prepare() 전에 불러두면 prepare 시점부터 바로 적용된다.
        block->convolution.loadImpulseResponse (std::move (ir), reader->sampleRate,
                                                juce::dsp::Convolution::Stereo::no,
                                                juce::dsp::Convolution::Trim::yes,
                                                juce::dsp::Convolution::Normalise::yes);
        return block;
    }

    void IrBlock::prepare (double sampleRate, int maxBlockSize)
    {
        convolution.prepare ({ sampleRate, (juce::uint32) maxBlockSize, 1 });
        rememberPrepare (sampleRate, maxBlockSize);
    }

    void IrBlock::process (const float* in, float* out, int numSamples) noexcept
    {
        juce::FloatVectorOperations::copy (out, in, numSamples);

        float* channels[1] = { out };
        juce::dsp::AudioBlock<float> audio (channels, 1, (size_t) numSamples);
        convolution.process (juce::dsp::ProcessContextReplacing<float> (audio));
    }
}
