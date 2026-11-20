#include "EqStage.h"

namespace rig
{
    namespace
    {
        constexpr double rampSeconds = 0.02;   // 값 변경과 스위치 전환 모두 약 20 ms
    }

    EqStage::EqStage (std::shared_ptr<NamParams> p) : params (std::move (p)) {}

    void EqStage::prepare (double sr, int)
    {
        sampleRate = sr;
        mixStep = 1.0 / juce::jmax (1.0, rampSeconds * sr);

        for (auto* s : { &bassDb, &middleDb, &trebleDb })
            s->reset (sr, rampSeconds);
        for (auto* s : { &bassHz, &middleHz, &trebleHz })
            s->reset (sr, rampSeconds);

        jumpToTargets();
        low.reset(); middle.reset(); high.reset();
        mix = params->eqOn.load (std::memory_order_relaxed) ? 1.0 : 0.0;   // 오디오 스레드가 쓰기 전이므로 바로 맞춘다
    }

    void EqStage::jumpToTargets() noexcept
    {
        bassDb.setCurrentAndTargetValue   (params->bassDb.load (std::memory_order_relaxed));
        middleDb.setCurrentAndTargetValue (params->middleDb.load (std::memory_order_relaxed));
        trebleDb.setCurrentAndTargetValue (params->trebleDb.load (std::memory_order_relaxed));
        bassHz.setCurrentAndTargetValue   (params->bassHz.load (std::memory_order_relaxed));
        middleHz.setCurrentAndTargetValue (params->middleHz.load (std::memory_order_relaxed));
        trebleHz.setCurrentAndTargetValue (params->trebleHz.load (std::memory_order_relaxed));
    }

    void EqStage::setTargets() noexcept
    {
        bassDb.setTargetValue   (params->bassDb.load (std::memory_order_relaxed));
        middleDb.setTargetValue (params->middleDb.load (std::memory_order_relaxed));
        trebleDb.setTargetValue (params->trebleDb.load (std::memory_order_relaxed));
        bassHz.setTargetValue   (params->bassHz.load (std::memory_order_relaxed));
        middleHz.setTargetValue (params->middleHz.load (std::memory_order_relaxed));
        trebleHz.setTargetValue (params->trebleHz.load (std::memory_order_relaxed));
    }

    void EqStage::process (float* io, int n) noexcept
    {
        const double target = params->eqOn.load (std::memory_order_relaxed) ? 1.0 : 0.0;

        if (mix <= 0.0 && target <= 0.0)
            return;   // 꺼져 있음: 신호를 건드리지 않는다

        if (mix <= 0.0)
        {
            // 0 에서 켜지기 시작: 옛 필터 상태와 옛 값을 버리고 지금 값에서 시작
            low.reset(); middle.reset(); high.reset();
            jumpToTargets();
        }

        // 값은 구간 단위로 따라가고 계수는 구간마다 한 번 계산한다
        setTargets();
        low.setLowShelf   (sampleRate, bassHz.skip (n),   NamParams::shelfQ, bassDb.skip (n));
        middle.setPeak    (sampleRate, middleHz.skip (n), NamParams::peakQ,  middleDb.skip (n));
        high.setHighShelf (sampleRate, trebleHz.skip (n), NamParams::shelfQ, trebleDb.skip (n));

        for (int i = 0; i < n; ++i)
        {
            const double x = io[i];
            const double y = high.process (middle.process (low.process (x)));

            mix = target > mix ? juce::jmin (target, mix + mixStep) : juce::jmax (target, mix - mixStep);
            io[i] = (float) (x * (1.0 - mix) + y * mix);   // 같은 신호의 두 버전이므로 선형 혼합
        }
    }
}
