#pragma once

#include <array>
#include <atomic>
#include <cmath>

namespace rig
{
    // 병렬의 합류 지점: 위/아래 두 갈래 출력에 각각 게인을 곱해 더한다.
    //
    // 규칙 (Block 의 On/Off 와 같은 방식)
    //  - setGains() : 메시지 스레드. immediately 는 오디오 스레드가 아직 쓰지 않는 Merge 에만 (새로 만든 병렬 등)
    //  - process()  : 오디오 스레드. 목표가 바뀌면 rampSamples 동안 선형으로 따라간다 (게인 0 → 1 이 rampSamples)
    //  - 갈래 Off 와 레벨은 RigModel 이 게인 하나로 합쳐서 넘긴다 (Off = 0)
    class Merge
    {
    public:
        void setGains (float upper, float lower, bool immediately = false) noexcept
        {
            target[0].store (upper, std::memory_order_relaxed);
            target[1].store (lower, std::memory_order_relaxed);

            if (immediately)
                current = { upper, lower };
        }

        // out = upper * 위 게인 + lower * 아래 게인. out 은 upper 와 같은 버퍼여도 된다 (샘플마다 계산)
        void process (const float* upper, const float* lower, float* out, int numSamples, int rampSamples) noexcept
        {
            const float targets[2] = { target[0].load (std::memory_order_relaxed),
                                       target[1].load (std::memory_order_relaxed) };
            const float step = 1.0f / (float) (rampSamples > 0 ? rampSamples : 1);

            for (int i = 0; i < numSamples; ++i)
            {
                for (size_t b = 0; b < 2; ++b)
                    current[b] = targets[b] > current[b] ? std::fmin (targets[b], current[b] + step)
                                                         : std::fmax (targets[b], current[b] - step);

                out[i] = upper[i] * current[0] + lower[i] * current[1];
            }
        }

    private:
        std::array<std::atomic<float>, 2> target {};
        std::array<float, 2> current {};   // 오디오 스레드 전용
    };
}
