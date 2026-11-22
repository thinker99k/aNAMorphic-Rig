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
    }
}
