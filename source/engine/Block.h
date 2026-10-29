#pragma once

#include <juce_core/juce_core.h>

#include <cmath>

namespace rig
{
    // 지금은 두 종류만. 공간계/다이나믹 등은 나중에 여기에 추가.
    enum class BlockType
    {
        Nam,   // .nam 캡처
        Ir     // .wav 임펄스 응답
    };

    inline juce::String toString (BlockType t)    { return t == BlockType::Nam ? "nam" : "ir"; }
    inline BlockType blockTypeFromString (const juce::String& s) { return s == "ir" ? BlockType::Ir : BlockType::Nam; }
    inline juce::String displayName (BlockType t) { return t == BlockType::Nam ? "NAM capture" : "Impulse response"; }

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

    protected:
        void rememberPrepare (double sr, int bs) noexcept { lastSampleRate = sr; lastBlockSize = bs; }

    private:
        double lastSampleRate = 0.0;
        int lastBlockSize = 0;
    };
}
