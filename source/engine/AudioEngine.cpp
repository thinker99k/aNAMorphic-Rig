#include "AudioEngine.h"

namespace rig
{

    AudioEngine::~AudioEngine()
    {
        live.store (nullptr);
    }

    AudioEngine::Program AudioEngine::Transition::result() const
    {
        Program p (prefix);
        p.insert (p.end(), newMiddle.begin(), newMiddle.end());
        p.insert (p.end(), suffix.begin(), suffix.end());
        return p;
    }

    template <typename Fn>
    void AudioEngine::Transition::forEachBlock (Fn&& fn) const
    {
        for (const auto* list : { &prefix, &oldMiddle, &newMiddle, &suffix })
            for (const auto& block : *list)
                fn (*block);
    }

    void AudioEngine::setProgram (Program program)
    {
        JUCE_ASSERT_MESSAGE_THREAD

        wanted = std::move (program);
        hasWanted = true;
        dispatch();
    }

    void AudioEngine::dispatch()
    {
        JUCE_ASSERT_MESSAGE_THREAD

        if (! hasWanted)
            return;

        const bool running = deviceRunning.load();

        // 페이드 중이면 끝날 때까지 기다린다 (collectGarbage 에서 다시 시도)
        if (running && owned != nullptr && settled.load (std::memory_order_acquire) != owned->id)
            return;

        const auto active = owned != nullptr ? owned->result() : Program();

        if (active == wanted)
        {
            hasWanted = false;
            return;
        }

        // 같은 인스턴스는 두 번 처리할 수 없다: 새 블록이 지금 소리에 들어 있으면(이동) 새 인스턴스가 올 때까지 기다린다
        size_t front = 0, back = 0;

        while (front < active.size() && front < wanted.size() && active[front] == wanted[front])
            ++front;

        while (back < active.size() - front && back < wanted.size() - front
               && active[active.size() - 1 - back] == wanted[wanted.size() - 1 - back])
            ++back;

        auto next = std::make_unique<Transition>();
        next->prefix.assign (active.begin(), active.begin() + (std::ptrdiff_t) front);
        next->oldMiddle.assign (active.begin() + (std::ptrdiff_t) front, active.end() - (std::ptrdiff_t) back);
        next->newMiddle.assign (wanted.begin() + (std::ptrdiff_t) front, wanted.end() - (std::ptrdiff_t) back);
        next->suffix.assign (active.end() - (std::ptrdiff_t) back, active.end());
        next->crossfade = running;

        for (const auto& block : next->newMiddle)
            if (std::find (active.begin(), active.end(), block) != active.end())
                return;

        // 장치 설정이 바뀐 뒤 로딩된 블록 등: 아직 오디오 스레드가 쓰지 않으므로 여기서 다시 준비해도 된다
        const auto sr = sampleRate.load();
        const auto bs = maxBlockSize.load();

        for (auto& block : next->newMiddle)
            if (block->needsPrepare (sr, bs))
                block->prepare (sr, bs);

        hasWanted = false;
        publish (std::move (next));
    }

    void AudioEngine::publish (std::unique_ptr<Transition> next)
    {
        next->id = nextTransitionId++;
        live.store (next.get(), std::memory_order_release);

        if (owned != nullptr)
            retired.push_back ({ std::move (owned), callbackCount.load (std::memory_order_acquire) });

        owned = std::move (next);

        // 장치가 멈춰 있으면 페이드할 것이 없다
        if (! deviceRunning.load())
            settled.store (owned->id, std::memory_order_release);
    }

    bool AudioEngine::isInUse (const Block* block) const
    {
        JUCE_ASSERT_MESSAGE_THREAD

        bool found = false;
        const auto check = [&] (const Block& b) { found = found || &b == block; };

        if (owned != nullptr)
            owned->forEachBlock (check);

        for (const auto& r : retired)
            r.transition->forEachBlock (check);

        return found;
    }

    void AudioEngine::collectGarbage()
    {
        JUCE_ASSERT_MESSAGE_THREAD

        const auto now = callbackCount.load (std::memory_order_acquire);
        const bool running = deviceRunning.load();

        // 교체 시점에 진행 중이던 콜백이 끝났다면(카운트가 2 이상 증가) 더 이상 옛 Transition 을 보지 않는다
        retired.erase (std::remove_if (retired.begin(), retired.end(),
                                       [&] (const Retired& r) { return ! running || now >= r.callbackStamp + 2; }),
                       retired.end());

        dispatch();
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

    void AudioEngine::audioDeviceAboutToStart (juce::AudioIODevice* device)
    {
        // 장치가 멈춘 상태에서 호출되므로 할당/Reset 이 허용된다
        const auto sr = device->getCurrentSampleRate();
        const auto bs = juce::jmax (1, device->getCurrentBufferSizeSamples());

        sampleRate.store (sr);
        maxBlockSize.store (bs);
        scratch.setSize (4, bs);
        fadeLength = juce::jmax (1, juce::roundToInt (sr * crossfadeSeconds));

        inputSmoothed.reset (sr, 0.02);
        outputSmoothed.reset (sr, 0.02);
        inputSmoothed.setCurrentAndTargetValue (inputGain.load (std::memory_order_relaxed));
        outputSmoothed.setCurrentAndTargetValue (outputGain.load (std::memory_order_relaxed));

        if (owned != nullptr)
            owned->forEachBlock ([sr, bs] (Block& block)
            {
                if (block.needsPrepare (sr, bs))
                    block.prepare (sr, bs);
            });

        // 멈췄다 다시 켜지므로 진행 중이던 페이드는 끝난 것으로 한다
        current = owned.get();
        fadePosition = fadeLength;
        settled.store (owned != nullptr ? owned->id : 0, std::memory_order_release);

        deviceRunning.store (true);
    }

    void AudioEngine::audioDeviceStopped()
    {
        deviceRunning.store (false);
    }

    void AudioEngine::processBlocks (const Program& blocks, float*& a, float*& b, int n) const noexcept
    {
        // 직렬: a -> 블록 -> b, 그다음 a/b 를 바꿔가며 진행 (결과는 항상 a). 꺼진 블록도 계산하고 dry 로 섞는다
        for (auto& block : blocks)
        {
            block->processMixed (a, b, n, fadeLength);
            std::swap (a, b);
        }
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
        if (auto* next = live.load (std::memory_order_acquire); next != current)
        {
            current = next;
            fadePosition = (next != nullptr && next->crossfade) ? 0 : fadeLength;
        }

        const int blockSize = juce::jmin (scratch.getNumSamples(), maxBlockSize.load());

        inputSmoothed.setTargetValue (inputGain.load (std::memory_order_relaxed));
        outputSmoothed.setTargetValue (outputGain.load (std::memory_order_relaxed));

        for (int offset = 0; offset < numSamples && blockSize > 0; offset += blockSize)
        {
            const int n = juce::jmin (blockSize, numSamples - offset);

            float* a = scratch.getWritePointer (0);
            float* b = scratch.getWritePointer (1);

            if (input != nullptr)
            {
                juce::FloatVectorOperations::copy (a, input + offset, n);

                // 입력 볼륨 (IN 미터는 게인 뒤에서 잰다 = 모델에 들어가는 레벨)
                inputSmoothed.applyGain (a, n);

                storeMax (inputPeak, juce::FloatVectorOperations::findMaximum (a, n));
                storeMax (inputPeak, -juce::FloatVectorOperations::findMinimum (a, n));
            }
            else
            {
                juce::FloatVectorOperations::clear (a, n);
                inputSmoothed.skip (n);
            }

            if (current != nullptr)
            {
                processBlocks (current->prefix, a, b, n);

                if (fadePosition < fadeLength)
                {
                    // 바뀐 구간만 옛/새 두 갈래로 같은 입력을 처리해서 등전력(sin/cos) 곡선으로 섞는다
                    float* c = scratch.getWritePointer (2);
                    float* d = scratch.getWritePointer (3);
                    juce::FloatVectorOperations::copy (c, a, n);

                    processBlocks (current->newMiddle, a, b, n);
                    processBlocks (current->oldMiddle, c, d, n);

                    const float step = 1.0f / (float) fadeLength;

                    for (int i = 0; i < n; ++i)
                    {
                        const float x = juce::jmin (1.0f, (float) (fadePosition + i) * step) * juce::MathConstants<float>::halfPi;
                        a[i] = a[i] * std::sin (x) + c[i] * std::cos (x);
                    }

                    fadePosition = juce::jmin (fadeLength, fadePosition + n);
                }
                else
                {
                    processBlocks (current->newMiddle, a, b, n);
                }

                processBlocks (current->suffix, a, b, n);
            }

            // 출력 볼륨 (OUT 미터는 게인 뒤에서 잰다)
            outputSmoothed.applyGain (a, n);

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

        if (current != nullptr && fadePosition >= fadeLength)
            settled.store (current->id, std::memory_order_release);

        callbackCount.fetch_add (1, std::memory_order_release);
    }
}
