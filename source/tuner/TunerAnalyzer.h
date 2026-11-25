#pragma once

#include "engine/AudioEngine.h"
#include "tuner/PitchDetector.h"
#include "tuner/TunerGauge.h"

#include <array>
#include <vector>

namespace rig
{
    // 튜너 분석 (튜너 모달과 Live 아래 줄 튜너가 같이 쓴다). 메시지 스레드에서 refreshHz 로 update 를 부른다
    //  - 엔진이 기록한 입력(지금 입력 채널)을 PitchDetector 로 분석, 최근 medianCount 개의 중앙값
    //  - 반음 이상 뛰면 새 음으로 보고 다시 모은다. silentAfter 프레임 연속 실패면 신호 없음 (그 전에는 마지막 값 유지)
    class TunerAnalyzer
    {
    public:
        static constexpr int refreshHz = 20;

        TunerState update (const AudioEngine& engine, int referenceHz);
        void reset();   // 켤 때 이전 값을 버린다

    private:
        static constexpr int medianCount = 5;   // 중앙값에 쓰는 최근 값 개수

        PitchDetector detector;
        std::vector<float> samples;
        double preparedRate = 0.0;

        std::array<double, medianCount> recent {};
        int recentCount = 0;
        int failedFrames = 0;
        TunerState state;
    };
}
