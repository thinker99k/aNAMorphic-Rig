#include "TunerAnalyzer.h"

#include <algorithm>
#include <cmath>

namespace rig
{
    namespace
    {
        // 동작 상수 (정확 범위는 TunerGauge::inTuneCents)
        constexpr int    silentAfter = 8;       // 연속 실패 프레임 (약 0.4 초)
        constexpr double jumpCents   = 100.0;   // 이만큼 뛰면 새 음으로 보고 다시 모은다
    }

    void TunerAnalyzer::reset()
    {
        recentCount = 0;
        failedFrames = 0;
        state = {};
    }

    TunerState TunerAnalyzer::update (const AudioEngine& engine, int referenceHz)
    {
        // 1. 샘플레이트가 바뀔 때만 다시 준비 (이때만 할당)
        const double sr = engine.getSampleRate();

        if (! juce::approximatelyEqual (sr, preparedRate))
        {
            detector.prepare (sr);
            samples.assign ((size_t) detector.samplesNeeded(), 0.0f);
            preparedRate = sr;
            recentCount = 0;
        }

        // 2-3. 최근 입력을 가져와 분석
        PitchResult result;

        if (engine.copyLatestInput (samples.data(), (int) samples.size()))
            result = detector.detect (samples.data(), (int) samples.size());

        if (result.valid)
        {
            // 4. 반음 이상 뛰면 새 음으로 보고 다시 모은다
            failedFrames = 0;

            if (recentCount > 0 && std::abs (1200.0 * std::log2 (result.hz / state.hz)) >= jumpCents)
                recentCount = 0;

            if (recentCount < medianCount)
            {
                recent[(size_t) recentCount++] = result.hz;
            }
            else
            {
                std::rotate (recent.begin(), recent.begin() + 1, recent.end());
                recent.back() = result.hz;
            }

            auto sorted = recent;
            std::sort (sorted.begin(), sorted.begin() + recentCount);
            const double hz = sorted[(size_t) recentCount / 2];
            const auto note = PitchDetector::toNote (hz, referenceHz);

            state = { true, hz, note.name, note.cents };
        }
        else if (++failedFrames >= silentAfter)
        {
            // 5. 8 프레임 연속이면 신호 없음. 그 전에는 마지막 값을 그대로 둔다
            state = {};
            recentCount = 0;
        }

        return state;
    }
}
