#pragma once

namespace rig
{
    // 블록이 자기 소리를 만든 직후 그 소리(wet)에 적용하는 처리. 블록마다 하나씩, 상태도 블록마다 따로
    class ParamStage
    {
    public:
        virtual ~ParamStage() = default;
        virtual void prepare (double sampleRate, int maxBlockSize) = 0;    // 오디오 스레드가 아닌 곳에서만
        virtual void process (float* inOut, int numSamples) noexcept = 0;  // 오디오 스레드: 할당/락/파일IO 금지
    };
}
