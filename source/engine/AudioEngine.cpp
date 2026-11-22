#include "AudioEngine.h"

namespace rig
{

    AudioEngine::~AudioEngine()
    {
        live.store (nullptr);
    }

    namespace
    {
        // 노드 하나와 그 안(병렬 갈래)의 모든 블록 / Merge
        template <typename Fn>
        void forEachNode (const AudioEngine::Program& program, Fn&& fn)
        {
            for (const auto& node : program)
            {
                fn (node);

                for (const auto& branch : node.branches)
                    forEachNode (branch, fn);
            }
        }

        // steps 안 모든 gap 의 한쪽 구간(part = &Step::oldPart 또는 &Step::newPart)의 노드
        template <typename StepList, typename Part, typename Fn>
        void forEachPartNode (const StepList& steps, Part part, Fn&& fn)
        {
            for (const auto& step : steps)
            {
                forEachNode (step.*part, fn);

                for (const auto& branch : step.branches)
                    forEachPartNode (branch, part, fn);
            }
        }

        // 같은 노드가 같은 순서로 (병렬은 갈래 내용까지)
        bool sameProgram (const AudioEngine::Program& a, const AudioEngine::Program& b)
        {
            return std::equal (a.begin(), a.end(), b.begin(), b.end(), [] (const auto& x, const auto& y)
            {
                return x.sameNode (y) && sameProgram (x.branches[0], y.branches[0]) && sameProgram (x.branches[1], y.branches[1]);
            });
        }
    }

    template <typename Fn>
    void AudioEngine::Transition::forEachBlock (Fn&& fn) const
    {
        const auto visit = [&] (const Node& node) { if (node.block != nullptr) fn (*node.block); };

        forEachNode (program, visit);
        forEachPartNode (steps, &Step::oldPart, visit);   // 페이드 중에만 처리하는 옛 구간
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

        static const Program none;
        const auto& active = owned != nullptr ? owned->program : none;
        const auto activeRouting = owned != nullptr ? owned->newRouting : Routing();

        if (sameProgram (active, wanted) && activeRouting == wantedRouting)
        {
            hasWanted = false;
            return;
        }

        auto next = std::make_unique<Transition>();
        next->program = wanted;
        next->oldRouting = activeRouting;
        next->newRouting = wantedRouting;
        next->crossfade = running;
        diff (active, wanted, next->steps);

        // 같은 인스턴스는 두 번 처리할 수 없다: 새 구간의 블록/Merge 가 지금 소리에 들어 있으면(이동 등) 새 인스턴스가 올 때까지 기다린다
        std::vector<const void*> activeNodes;
        forEachNode (active, [&] (const Node& node) { activeNodes.push_back (node.block != nullptr ? (const void*) node.block.get()
                                                                                                    : (const void*) node.merge.get()); });

        bool conflict = false;
        std::vector<Block*> added;

        const auto checkNew = [&] (const Node& node)
        {
            const void* key = node.block != nullptr ? (const void*) node.block.get() : (const void*) node.merge.get();
            conflict = conflict || std::find (activeNodes.begin(), activeNodes.end(), key) != activeNodes.end();

            if (node.block != nullptr)
                added.push_back (node.block.get());
        };

        forEachPartNode (next->steps, &Step::newPart, checkNew);

        if (conflict)
            return;

        // 장치 설정이 바뀐 뒤 로딩된 블록 등: 아직 오디오 스레드가 쓰지 않으므로 여기서 다시 준비해도 된다
        const auto sr = sampleRate.load();
        const auto bs = maxBlockSize.load();

        for (auto* block : added)
            if (block->needsPrepare (sr, bs))
                block->prepare (sr, bs);

        if (! running)
            next->steps.clear();   // 페이드가 없으면 옛 구간을 붙잡아 둘 이유가 없다

        hasWanted = false;
        publish (std::move (next));
    }

    void AudioEngine::diff (const Program& oldList, const Program& newList, std::vector<Step>& out)
    {
        // 양쪽에 같은 순서로 남은 노드 = 최장 공통 부분열 (노드 수가 적어 O(n*m) 표로 충분)
        const size_t n = oldList.size(), m = newList.size();
        std::vector<size_t> lcs ((n + 1) * (m + 1), 0);
        const auto at = [&] (size_t i, size_t j) -> size_t& { return lcs[i * (m + 1) + j]; };

        for (size_t i = n; i-- > 0;)
            for (size_t j = m; j-- > 0;)
                at (i, j) = oldList[i].sameNode (newList[j]) ? at (i + 1, j + 1) + 1 : std::max (at (i + 1, j), at (i, j + 1));

        Step gap;
        gap.gap = true;

        const auto flushGap = [&]
        {
            if (! gap.oldPart.empty() || ! gap.newPart.empty())
                out.push_back (std::move (gap));

            gap = {};
            gap.gap = true;
        };

        size_t i = 0, j = 0;

        while (i < n || j < m)
        {
            if (i < n && j < m && oldList[i].sameNode (newList[j]))
            {
                flushGap();

                Step keep;
                keep.node = newList[j];

                if (keep.node.isParallel())
                    for (size_t b = 0; b < 2; ++b)
                        diff (oldList[i].branches[b], newList[j].branches[b], keep.branches[b]);

                out.push_back (std::move (keep));
                ++i;
                ++j;
            }
            else if (j < m && (i == n || at (i, j + 1) >= at (i + 1, j)))
            {
                gap.newPart.push_back (newList[j++]);
            }
            else
            {
                gap.oldPart.push_back (oldList[i++]);
            }
        }

        flushGap();
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
        scratch.setSize (numScratchPairs * 2, bs);
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

    float AudioEngine::fadeAngle (int i) const noexcept
    {
        return juce::jmin (1.0f, (float) (fadePosition + i) / (float) fadeLength) * juce::MathConstants<float>::halfPi;
    }

    void AudioEngine::processProgram (const Program& program, float*& a, float*& b, int free, int n) noexcept
    {
        // 직렬: a -> 블록 -> b, 그다음 a/b 를 바꿔가며 진행 (결과는 항상 a). 꺼진 블록도 계산하고 dry 로 섞는다
        for (const auto& node : program)
        {
            if (! node.isParallel())
            {
                node.block->processMixed (a, b, n, fadeLength);
                std::swap (a, b);
                continue;
            }

            if (free >= numScratchPairs) { jassertfalse; continue; }   // 병렬이 한 단계면 생기지 않는다

            // 병렬: 아래 갈래는 입력을 복사한 다음 버퍼 쌍에서, 위 갈래는 a 에서 처리하고 Merge 로 합친다
            float* c = scratch.getWritePointer (free * 2);
            float* d = scratch.getWritePointer (free * 2 + 1);
            juce::FloatVectorOperations::copy (c, a, n);

            processProgram (node.branches[0], a, b, free + 1, n);
            processProgram (node.branches[1], c, d, free + 1, n);
            node.merge->process (a, c, a, n, fadeLength);
        }
    }

    void AudioEngine::processSteps (const std::vector<Step>& steps, float*& a, float*& b, int free, int n) noexcept
    {
        for (const auto& step : steps)
        {
            if (! step.gap && ! step.node.isParallel())
            {
                step.node.block->processMixed (a, b, n, fadeLength);
                std::swap (a, b);
                continue;
            }

            if (free >= numScratchPairs) { jassertfalse; continue; }

            float* c = scratch.getWritePointer (free * 2);
            float* d = scratch.getWritePointer (free * 2 + 1);
            juce::FloatVectorOperations::copy (c, a, n);

            if (step.gap)
            {
                // 달라진 구간: 같은 입력으로 새 구간(a)과 옛 구간(c)을 함께 돌려 등전력으로 섞는다. 새 쪽 = sin, 옛 쪽 = cos
                processProgram (step.newPart, a, b, free + 1, n);
                processProgram (step.oldPart, c, d, free + 1, n);

                for (int i = 0; i < n; ++i)
                    a[i] = a[i] * std::sin (fadeAngle (i)) + c[i] * std::cos (fadeAngle (i));
            }
            else
            {
                // 남은 병렬: 갈래 안에서도 달라진 구간만 섞는다
                processSteps (step.branches[0], a, b, free + 1, n);
                processSteps (step.branches[1], c, d, free + 1, n);
                step.node.merge->process (a, c, a, n, fadeLength);
            }
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

    // [SPECTRUM] begin (테스트용)
    void AudioEngine::ScopeRing::write (const float* samples, int n) noexcept
    {
        const auto pos = writePos.load (std::memory_order_relaxed);   // 쓰는 쪽은 오디오 스레드 하나

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
        return writePos.load (std::memory_order_acquire) - start <= (juce::uint64) size;
    }
    // [SPECTRUM] end

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

            float* a = scratch.getWritePointer (0);
            float* b = scratch.getWritePointer (1);

            if (fading && newInput != oldInput)
            {
                // 입력 채널이 바뀌었다: 옛 채널에서 새 채널로 등전력으로 넘어간다 (새 쪽 = sin, 옛 쪽 = cos, 없는 쪽은 무음)
                for (int i = 0; i < n; ++i)
                    a[i] = (newInput != nullptr ? newInput[offset + i] * std::sin (fadeAngle (i)) : 0.0f)
                         + (oldInput != nullptr ? oldInput[offset + i] * std::cos (fadeAngle (i)) : 0.0f);
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
                if (fading)
                    processSteps (current->steps, a, b, 1, n);   // 버퍼 쌍 0 = a, b
                else
                    processProgram (current->program, a, b, 1, n);
            }

            // 출력 볼륨 (OUT 미터는 게인 뒤에서 잰다)
            outputSmoothed.applyGain (a, n);

            storeMax (outputPeak, juce::FloatVectorOperations::findMaximum (a, n));
            storeMax (outputPeak, -juce::FloatVectorOperations::findMinimum (a, n));

            // [SPECTRUM] begin (테스트용) 스펙트럼 창용 기록: 게인 뒤, 자르지 않은 값 그대로 (창이 닫혀 있으면 아무것도 안 함)
            if (scopeEnabled.load (std::memory_order_relaxed))
                outputScope.write (a, n);
            // [SPECTRUM] end

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
                        out[offset + i] = a[i] * (inNew ? std::sin (fadeAngle (i)) : std::cos (fadeAngle (i)));
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
