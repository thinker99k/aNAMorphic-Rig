#pragma once

#include <juce_core/juce_core.h>

#include <algorithm>
#include <atomic>
#include <cmath>

namespace rig
{
    // 지금은 두 종류만. 공간계/다이나믹 등은 나중에 여기에 추가.
    enum class BlockType
    {
        None,  // 미지정 블록 (통과, 처리 없음)
        Nam,   // .nam 캡처
        Ir     // .wav 임펄스 응답
    };

    inline juce::String toString (BlockType t)
    {
        switch (t)
        {
            case BlockType::Nam:  return "nam";
            case BlockType::Ir:   return "ir";
            case BlockType::None: break;
        }

        return "none";
    }

    // 모르는 값은 미지정 블록으로
    inline BlockType blockTypeFromString (const juce::String& s)
    {
        if (s == "nam") return BlockType::Nam;
        if (s == "ir")  return BlockType::Ir;
        return BlockType::None;
    }

    // 파일을 지정해야 하는 종류인지 (NAM / IR)
    inline bool usesFile (BlockType t) { return t == BlockType::Nam || t == BlockType::Ir; }

    // 체인에 들어가는 DSP 블록의 공통 인터페이스.
    //
    // 규칙
    //  - prepare() : 오디오 스레드가 아닌 곳에서만 호출. 메모리 할당은 여기서만.
    //  - process() : 오디오 스레드에서 호출. 할당/락/파일IO 금지.
    //  - 지금은 모노 입력 -> 모노 출력만 다룬다 (기타 신호 체인 기준).
    //  - in 과 out 은 항상 서로 다른 버퍼로 넘어온다.
    class Block
    {
    public:
        virtual ~Block() = default;

        virtual BlockType type() const noexcept = 0;
        virtual void prepare (double sampleRate, int maxBlockSize) = 0;
        virtual void process (const float* in, float* out, int numSamples) noexcept = 0;

        // 블록이 원하는 샘플레이트 (상관없으면 0)
        virtual double expectedSampleRate() const noexcept { return 0.0; }

        double preparedSampleRate() const noexcept { return lastSampleRate; }
        int preparedBlockSize() const noexcept     { return lastBlockSize; }

        // 장치 설정이 바뀌어 prepare() 를 다시 해야 하는지
        bool needsPrepare (double sampleRate, int maxBlockSize) const noexcept
        {
            return std::abs (lastSampleRate - sampleRate) > 0.5 || lastBlockSize != maxBlockSize;
        }

        // On/Off = 블록 출력(wet)과 입력(dry)을 섞는 비율. 꺼져 있어도 블록은 계속 계산한다
        // (NAM 은 내부 상태가 있어서, 멈췄다 켜면 옛 상태가 잠깐 들린다)
        //  - setOn() : 메시지 스레드. immediately 는 오디오 스레드가 아직 쓰지 않는 블록에만 (새로 로딩된 블록 등)
        //  - processMixed() : 오디오 스레드. 목표가 바뀌면 rampSamples 동안 등전력(sin/cos) 곡선으로 넘어간다
        void setOn (bool shouldBeOn, bool immediately = false) noexcept
        {
            onTarget.store (shouldBeOn, std::memory_order_relaxed);
            if (immediately)
                mix = shouldBeOn ? 1.0f : 0.0f;
        }

        void processMixed (const float* in, float* out, int numSamples, int rampSamples) noexcept
        {
            process (in, out, numSamples);

            const bool on = onTarget.load (std::memory_order_relaxed);

            if (on && mix >= 1.0f)
                return;

            if (! on && mix <= 0.0f)
            {
                std::copy (in, in + numSamples, out);   // 꺼진 상태 유지: 계산은 했고 출력은 dry
                return;
            }

            const float target = on ? 1.0f : 0.0f;

            const float step = 1.0f / (float) (rampSamples > 0 ? rampSamples : 1);
            constexpr float halfPi = 1.57079632679489661923f;

            for (int i = 0; i < numSamples; ++i)
            {
                mix = target > mix ? std::fmin (target, mix + step) : std::fmax (target, mix - step);
                out[i] = out[i] * std::sin (mix * halfPi) + in[i] * std::cos (mix * halfPi);
            }
        }

    protected:
        void rememberPrepare (double sr, int bs) noexcept { lastSampleRate = sr; lastBlockSize = bs; }

    private:
        std::atomic<bool> onTarget { true };
        float mix = 1.0f;   // 오디오 스레드 전용: 0 = dry, 1 = wet

        double lastSampleRate = 0.0;
        int lastBlockSize = 0;
    };
}
