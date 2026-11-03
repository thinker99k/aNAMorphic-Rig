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

    void AudioEngine::setInputGainDb (float db)
    {
        JUCE_ASSERT_MESSAGE_THREAD
        // 입력 최솟값(-15 dB)은 실제 감쇠이므로 -inf 처리를 하지 않는다 (JUCE 기본 기준 -100 dB)
        inputGain.store (juce::Decibels::decibelsToGain (db), std::memory_order_relaxed);
    }

    void AudioEngine::setOutputGainDb (float db)
    {
        JUCE_ASSERT_MESSAGE_THREAD
        outputGain.store (juce::Decibels::decibelsToGain (db, outputMinDb), std::memory_order_relaxed);
    }

    void AudioEngine::ScopeRing::write (const float* samples, int n) noexcept
    {
        const auto pos = writePos.load (std::memory_order_relaxed);   // 쓰는 쪽은 이 스레드 하나

        for (int i = 0; i < n; ++i)
            data[(size_t) ((pos + (juce::uint64) i) & (size - 1))].store (samples[i], std::memory_order_relaxed);

        writePos.store (pos + (juce::uint64) n, std::memory_order_release);
    }

    bool AudioEngine::ScopeRing::copyLatest (float* dest, int numSamples) const noexcept
    {
        const auto end = writePos.load (std::memory_order_acquire);

        if (numSamples <= 0 || numSamples > size / 2 || end < (juce::uint64) numSamples)
            return false;

        const auto start = end - (juce::uint64) numSamples;

        for (int i = 0; i < numSamples; ++i)
            dest[i] = data[(size_t) ((start + (juce::uint64) i) & (size - 1))].load (std::memory_order_relaxed);

        // 복사하는 사이 오디오 스레드가 start 구간을 덮어썼다면 이번 프레임은 버린다
        const auto newEnd = writePos.load (std::memory_order_acquire);
        return newEnd - start <= (juce::uint64) size;
    }

    void AudioEngine::audioDeviceAboutToStart (juce::AudioIODevice* device)
    {
        // 장치가 멈춘 상태에서 호출되므로 할당/Reset 이 허용된다
        const auto sr = device->getCurrentSampleRate();
        const auto bs = juce::jmax (1, device->getCurrentBufferSizeSamples());

        sampleRate.store (sr);
        maxBlockSize.store (bs);
        scratch.setSize (2, bs);

        inputSmoothed.reset (sr, 0.02);
        outputSmoothed.reset (sr, 0.02);
        inputSmoothed.setCurrentAndTargetValue (inputGain.load (std::memory_order_relaxed));
        outputSmoothed.setCurrentAndTargetValue (outputGain.load (std::memory_order_relaxed));

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

        inputSmoothed.setTargetValue (inputGain.load (std::memory_order_relaxed));
        outputSmoothed.setTargetValue (outputGain.load (std::memory_order_relaxed));

        const bool scopeOn = scopeEnabled.load (std::memory_order_relaxed);

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

            // 입력 파형: 게인 앞, 입력이 없으면 0 이 쌓인다
            if (scopeOn)
                inputScope.write (a, n);

            // 입력 볼륨 (IN 미터는 게인 앞에서 잰다)
            inputSmoothed.applyGain (a, n);

            // 직렬 체인: a -> 블록 -> b, 그다음 a/b 를 바꿔가며 진행
            if (chain != nullptr)
            {
                for (auto& block : chain->blocks)
                {
                    block->process (a, b, n);
                    std::swap (a, b);
                }
            }

            // 출력 볼륨 (OUT 미터는 게인 뒤에서 잰다)
            outputSmoothed.applyGain (a, n);

            storeMax (outputPeak, juce::FloatVectorOperations::findMaximum (a, n));
            storeMax (outputPeak, -juce::FloatVectorOperations::findMinimum (a, n));

            // 출력 파형: 게인 뒤, 클리핑 전 값 그대로
            if (scopeOn)
                outputScope.write (a, n);

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
