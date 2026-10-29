#include "AudioEngine.h"

namespace rig
{
    AudioEngine::~AudioEngine()
    {
        live.store (nullptr);
    }

    void AudioEngine::setChain (std::unique_ptr<Chain> newChain)
    {
        JUCE_ASSERT_MESSAGE_THREAD

        live.store (newChain.get(), std::memory_order_release);

        if (owned != nullptr)
            retired.push_back ({ std::move (owned), callbackCount.load (std::memory_order_acquire) });

        owned = std::move (newChain);
        collectGarbage();
    }

    void AudioEngine::collectGarbage()
    {
        JUCE_ASSERT_MESSAGE_THREAD

        const auto now = callbackCount.load (std::memory_order_acquire);
        const bool running = deviceRunning.load();

        // 교체 시점에 진행 중이던 콜백이 끝났다면(카운트가 2 이상 증가) 더 이상 옛 체인을 보지 않는다
        retired.erase (std::remove_if (retired.begin(), retired.end(),
                                       [&] (const Retired& r) { return ! running || now >= r.callbackStamp + 2; }),
                       retired.end());
    }

    void AudioEngine::audioDeviceAboutToStart (juce::AudioIODevice* device)
    {
        // 장치가 멈춘 상태에서 호출되므로 할당/Reset 이 허용된다
        const auto sr = device->getCurrentSampleRate();
        const auto bs = juce::jmax (1, device->getCurrentBufferSizeSamples());

        sampleRate.store (sr);
        maxBlockSize.store (bs);
        scratch.setSize (2, bs);

        if (owned != nullptr)
            for (auto& block : owned->blocks)
                if (block->needsPrepare (sr, bs))
                    block->prepare (sr, bs);

        deviceRunning.store (true);
    }

    void AudioEngine::audioDeviceStopped()
    {
        deviceRunning.store (false);
    }

    void AudioEngine::storeMax (std::atomic<float>& target, float value) noexcept
    {
        auto current = target.load (std::memory_order_relaxed);
        while (value > current && ! target.compare_exchange_weak (current, value, std::memory_order_relaxed)) {}
    }

    void AudioEngine::audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                                        int numInputChannels,
                                                        float* const* outputChannelData,
                                                        int numOutputChannels,
                                                        int numSamples,
                                                        const juce::AudioIODeviceCallbackContext&)
    {
        juce::ScopedNoDenormals noDenormals;

        // 설정창에서 고른 입력 채널 하나만 열어두므로, 활성 입력의 0 번이 곧 기타 입력
        const float* input = (numInputChannels > 0) ? inputChannelData[0] : nullptr;
        auto* chain = live.load (std::memory_order_acquire);

        const int blockSize = juce::jmin (scratch.getNumSamples(), maxBlockSize.load());

        for (int offset = 0; offset < numSamples && blockSize > 0; offset += blockSize)
        {
            const int n = juce::jmin (blockSize, numSamples - offset);

            float* a = scratch.getWritePointer (0);
            float* b = scratch.getWritePointer (1);

            if (input != nullptr)
            {
                juce::FloatVectorOperations::copy (a, input + offset, n);
                storeMax (inputPeak, juce::FloatVectorOperations::findMaximum (a, n));
                storeMax (inputPeak, -juce::FloatVectorOperations::findMinimum (a, n));
            }
            else
            {
                juce::FloatVectorOperations::clear (a, n);
            }

            // 직렬 체인: a -> 블록 -> b, 그다음 a/b 를 바꿔가며 진행
            if (chain != nullptr)
            {
                for (auto& block : chain->blocks)
                {
                    block->process (a, b, n);
                    std::swap (a, b);
                }
            }

            storeMax (outputPeak, juce::FloatVectorOperations::findMaximum (a, n));
            storeMax (outputPeak, -juce::FloatVectorOperations::findMinimum (a, n));

            // 모노 결과를 열려 있는 모든 출력 채널(보통 L/R)에 복사
            for (int ch = 0; ch < numOutputChannels; ++ch)
                if (auto* out = outputChannelData[ch])
                    juce::FloatVectorOperations::copy (out + offset, a, n);
        }

        // 버퍼가 준비되지 않은 예외 상황에서는 무음
        if (blockSize <= 0)
            for (int ch = 0; ch < numOutputChannels; ++ch)
                if (auto* out = outputChannelData[ch])
                    juce::FloatVectorOperations::clear (out, numSamples);

        callbackCount.fetch_add (1, std::memory_order_release);
    }
}
