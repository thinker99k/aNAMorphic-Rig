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

    void AudioEngine::setProgram (Program program, Routing routing)
    {
        JUCE_ASSERT_MESSAGE_THREAD

        wanted = std::move (program);
        wantedRouting = routing;
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
        const auto activeRouting = owned != nullptr ? owned->newRouting : Routing();

        if (active == wanted && activeRouting == wantedRouting)
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
        next->oldRouting = activeRouting;
        next->newRouting = wantedRouting;
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

    AudioEngine::Channels AudioEngine::resolve (const Routing& r, int numInputs, int numOutputs) noexcept
    {
        Channels c;

        if (numInputs > 0)
            c.input = juce::isPositiveAndBelow (r.input, numInputs) ? r.input : 0;

        const int width = r.stereoOutput ? 2 : 1;

        if (r.output >= 0 && r.output + width <= numOutputs)
        {
            c.firstOutput = r.output;
            c.numOutputs = width;
        }
        else
        {
            c.numOutputs = juce::jmin (2, numOutputs);   // 기본: 1+2, 채널이 하나뿐이면 1
        }

        return c;
    }

    void AudioEngine::audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                                        int numInputChannels,
                                                        float* const* outputChannelData,
                                                        int numOutputChannels,
                                                        int numSamples,
                                                        const juce::AudioIODeviceCallbackContext&)
    {
        juce::ScopedNoDenormals noDenormals;

        if (auto* next = live.load (std::memory_order_acquire); next != current)
        {
            current = next;
            fadePosition = (next != nullptr && next->crossfade) ? 0 : fadeLength;
        }

        // 장치의 모든 채널을 열어 두므로 콜백 배열 번호 = 장치 채널 번호
        const auto newChannels = resolve (current != nullptr ? current->newRouting : Routing(), numInputChannels, numOutputChannels);
        const auto oldChannels = resolve (current != nullptr ? current->oldRouting : Routing(), numInputChannels, numOutputChannels);
        const float* newInput = newChannels.input >= 0 ? inputChannelData[newChannels.input] : nullptr;
        const float* oldInput = oldChannels.input >= 0 ? inputChannelData[oldChannels.input] : nullptr;

        const int blockSize = juce::jmin (scratch.getNumSamples(), maxBlockSize.load());

        inputSmoothed.setTargetValue (inputGain.load (std::memory_order_relaxed));
        outputSmoothed.setTargetValue (outputGain.load (std::memory_order_relaxed));

        for (int offset = 0; offset < numSamples && blockSize > 0; offset += blockSize)
        {
            const int n = juce::jmin (blockSize, numSamples - offset);
            const bool fading = current != nullptr && fadePosition < fadeLength;

            // 등전력 곡선: 새 쪽 = sin, 옛 쪽 = cos
            const float step = 1.0f / (float) fadeLength;
            const auto angle = [&] (int i)
            {
                return juce::jmin (1.0f, (float) (fadePosition + i) * step) * juce::MathConstants<float>::halfPi;
            };

            float* a = scratch.getWritePointer (0);
            float* b = scratch.getWritePointer (1);

            if (fading && newInput != oldInput)
            {
                // 입력 채널이 바뀌었다: 옛 채널에서 새 채널로 넘어간다 (없는 쪽은 무음)
                for (int i = 0; i < n; ++i)
                    a[i] = (newInput != nullptr ? newInput[offset + i] * std::sin (angle (i)) : 0.0f)
                         + (oldInput != nullptr ? oldInput[offset + i] * std::cos (angle (i)) : 0.0f);
            }
            else if (newInput != nullptr)
            {
                juce::FloatVectorOperations::copy (a, newInput + offset, n);
            }
            else
            {
                juce::FloatVectorOperations::clear (a, n);
            }

            // 입력 볼륨 (IN 미터는 게인 뒤에서 잰다 = 모델에 들어가는 레벨)
            inputSmoothed.applyGain (a, n);

            storeMax (inputPeak, juce::FloatVectorOperations::findMaximum (a, n));
            storeMax (inputPeak, -juce::FloatVectorOperations::findMinimum (a, n));

            if (current != nullptr)
            {
                processBlocks (current->prefix, a, b, n);

                if (fading)
                {
                    // 바뀐 구간만 옛/새 두 갈래로 같은 입력을 처리해서 섞는다
                    float* c = scratch.getWritePointer (2);
                    float* d = scratch.getWritePointer (3);
                    juce::FloatVectorOperations::copy (c, a, n);

                    processBlocks (current->newMiddle, a, b, n);
                    processBlocks (current->oldMiddle, c, d, n);

                    for (int i = 0; i < n; ++i)
                        a[i] = a[i] * std::sin (angle (i)) + c[i] * std::cos (angle (i));
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

            // 모노 결과를 고른 출력 채널(보통 1+2)에 복사하고 나머지 채널은 무음.
            // 페이드 중 새 채널에만 있으면 키우고(sin), 옛 채널에만 있으면 줄인다(cos)
            for (int ch = 0; ch < numOutputChannels; ++ch)
            {
                auto* out = outputChannelData[ch];
                if (out == nullptr)
                    continue;

                const bool inNew = newChannels.hasOutput (ch);
                const bool inOld = fading && oldChannels.hasOutput (ch);

                if (inNew && (inOld || ! fading))
                    juce::FloatVectorOperations::copy (out + offset, a, n);
                else if (inNew || inOld)
                    for (int i = 0; i < n; ++i)
                        out[offset + i] = a[i] * (inNew ? std::sin (angle (i)) : std::cos (angle (i)));
                else
                    juce::FloatVectorOperations::clear (out + offset, n);
            }

            if (fading)
                fadePosition = juce::jmin (fadeLength, fadePosition + n);
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
