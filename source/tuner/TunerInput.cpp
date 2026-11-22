#include "TunerInput.h"

namespace rig
{
    bool TunerInput::copyLatest (float* dest, int numSamples) const noexcept
    {
        const auto end = writePos.load (std::memory_order_acquire);

        if (dest == nullptr || numSamples <= 0 || numSamples > ringSize / 2 || end < (juce::uint64) numSamples)
            return false;

        const auto start = end - (juce::uint64) numSamples;

        for (int i = 0; i < numSamples; ++i)
            dest[i] = data[(start + (juce::uint64) i) & (ringSize - 1)].load (std::memory_order_relaxed);

        // 복사하는 동안 쓰기가 한 바퀴를 넘었으면 앞부분이 덮어써졌다
        const auto newEnd = writePos.load (std::memory_order_acquire);
        return newEnd - start <= (juce::uint64) ringSize;
    }

    void TunerInput::audioDeviceAboutToStart (juce::AudioIODevice* device)
    {
        if (device == nullptr)
            return;

        sampleRate.store (device->getCurrentSampleRate(), std::memory_order_relaxed);
        inputChannel.store (resolveChannel (device->getActiveInputChannels().countNumberOfSetBits()) + 1, std::memory_order_relaxed);
        writePos.store (0, std::memory_order_release);

        passGain.reset (device->getCurrentSampleRate(), fadeSeconds);
        passGain.setCurrentAndTargetValue (0.0f);
    }

    void TunerInput::audioDeviceStopped()
    {
    }

    void TunerInput::audioDeviceIOCallbackWithContext (const float* const* inputChannelData, int numInputChannels,
                                                       float* const* outputChannelData, int numOutputChannels,
                                                       int numSamples, const juce::AudioIODeviceCallbackContext&)
    {
        juce::ScopedNoDenormals noDenormals;

        for (int ch = 0; ch < numOutputChannels; ++ch)
            if (outputChannelData[ch] != nullptr)
                juce::FloatVectorOperations::clear (outputChannelData[ch], numSamples);

        const int ch = resolveChannel (numInputChannels);
        inputChannel.store (ch + 1, std::memory_order_relaxed);

        if (ch < 0 || inputChannelData[ch] == nullptr)
            return;

        const float* x = inputChannelData[ch];
        const auto pos = writePos.load (std::memory_order_relaxed);

        for (int i = 0; i < numSamples; ++i)
            data[(pos + (juce::uint64) i) & (ringSize - 1)].store (x[i], std::memory_order_relaxed);

        writePos.store (pos + (juce::uint64) numSamples, std::memory_order_release);

        // 입력 통과: 패치의 출력 채널로 (없으면 1+2, 채널이 하나뿐이면 1)
        passGain.setTargetValue (passThrough.load (std::memory_order_relaxed) ? 1.0f : 0.0f);

        if (! passGain.isSmoothing() && passGain.getCurrentValue() <= 0.0f)
            return;

        const int width = stereoOutput.load (std::memory_order_relaxed) ? 2 : 1;
        int first = outputChannel.load (std::memory_order_relaxed);
        int count = width;

        if (first < 0 || first + width > numOutputChannels)
        {
            first = 0;
            count = juce::jmin (2, numOutputChannels);
        }

        for (int i = 0; i < numSamples; ++i)
        {
            const float y = x[i] * passGain.getNextValue();

            for (int o = first; o < first + count; ++o)
                if (outputChannelData[o] != nullptr)
                    outputChannelData[o][i] = y;
        }
    }
}
