#pragma once

#include <vector>

namespace rig
{
    struct PitchResult
    {
        bool   valid = false;
        double hz    = 0.0;     // valid 일 때만 의미 있음
    };

    struct NoteInfo
    {
        int         midi   = 0;      // 가장 가까운 음의 MIDI 번호 (A4 = 69)
        const char* name   = "";     // "C", "C#", ..., "B"
        double      cents  = 0.0;    // -50 ~ +50
    };

    // YIN 피치 검출. UI 스레드 전용, 메모리 할당은 prepare 에서만
    class PitchDetector
    {
    public:
        static constexpr int    window     = 2048;     // 분석 구간 길이 (샘플)
        static constexpr double minHz      = 60.0;
        static constexpr double maxHz      = 1500.0;
        static constexpr float  threshold  = 0.15f;    // YIN 임계값
        static constexpr float  silenceRms = 0.002f;   // 이보다 작으면 소리 없음 (약 -54 dBFS)

        void prepare (double sampleRate);                       // 버퍼 크기 결정. 샘플레이트가 바뀔 때만 부른다
        int  samplesNeeded() const noexcept;                    // window + tauMax + 2
        PitchResult detect (const float* x, int n) noexcept;    // n >= samplesNeeded()

        // referenceHz = A4 기준음 (예: 440.0)
        static NoteInfo toNote (double hz, double referenceHz) noexcept;

    private:
        double sampleRate = 48000.0;
        int tauMin = 2, tauMax = 0;
        std::vector<float> d, cmnd;   // 크기 tauMax + 2 (cmnd[t + 1] 까지 읽는다)
    };
}
