#include "CutStage.h"

#include <cmath>

namespace rig
{
    namespace
    {
        constexpr double rampSeconds = transitionSeconds;   // 값 변경과 전원 전환 모두 20 ms (Block.h)
    }

    CutStage::CutStage (std::shared_ptr<BlockParams> p) : params (std::move (p))
    {
        lowCut.highPass = true;

        // Butterworth n 차의 단별 Q: Q_k = 1 / (2 sin((2k-1) pi / 2n)), k = 1..n/2
        // n = 4 이면 k = 1..2 순서로 1.3066, 0.5412 (직렬 필터는 순서와 무관하게 같은 응답)
        for (int k = 1; k <= numStages; ++k)
            q[(size_t) k - 1] = 1.0 / (2.0 * std::sin ((2.0 * k - 1.0) * juce::MathConstants<double>::pi / (2.0 * IrParams::cutOrder)));
    }

    void CutStage::prepare (double sr, int)
    {
        sampleRate = sr;
        mixStep = 1.0 / juce::jmax (1.0, rampSeconds * sr);

        // 오디오 스레드가 쓰기 전이므로 바로 맞춘다
        lowCut.hz.reset (sr, rampSeconds);
        lowCut.hz.setCurrentAndTargetValue (params->get (IrParams::lowCutHz));
        lowCut.mix = params->isOn (IrParams::lowCutOn) ? 1.0 : 0.0;

        highCut.hz.reset (sr, rampSeconds);
        highCut.hz.setCurrentAndTargetValue (params->get (IrParams::highCutHz));
        highCut.mix = params->isOn (IrParams::highCutOn) ? 1.0 : 0.0;

        for (auto* s : { &lowCut, &highCut })
            for (auto& b : s->stages)
                b.reset();
    }

    void CutStage::process (float* io, int n) noexcept
    {
        processSection (lowCut,  params->isOn (IrParams::lowCutOn),
                        params->get (IrParams::lowCutHz), io, n);
        processSection (highCut, params->isOn (IrParams::highCutOn),
                        params->get (IrParams::highCutHz), io, n);
    }

    void CutStage::processSection (Section& s, bool on, double targetHz, float* io, int n) noexcept
    {
        const double target = on ? 1.0 : 0.0;

        if (s.mix <= 0.0 && target <= 0.0)
            return;   // 꺼져 있음: 신호를 건드리지 않는다

        if (s.mix <= 0.0)
        {
            // 0 에서 켜지기 시작: 옛 필터 상태와 옛 값을 버리고 지금 값에서 시작
            for (auto& b : s.stages)
                b.reset();
            s.hz.setCurrentAndTargetValue (targetHz);
        }

        // 값은 구간 단위로 따라가고 계수는 구간마다 한 번 계산한다 (모든 단이 같은 주파수, Q 만 다름)
        s.hz.setTargetValue (targetHz);
        const double hz = s.hz.skip (n);

        for (int k = 0; k < numStages; ++k)
        {
            if (s.highPass)
                s.stages[(size_t) k].setHighPass (sampleRate, hz, q[(size_t) k]);
            else
                s.stages[(size_t) k].setLowPass (sampleRate, hz, q[(size_t) k]);
        }

        for (int i = 0; i < n; ++i)
        {
            const double x = io[i];
            double y = x;

            for (auto& b : s.stages)
                y = b.process (y);

            s.mix = target > s.mix ? juce::jmin (target, s.mix + mixStep) : juce::jmax (target, s.mix - mixStep);
            io[i] = (float) (x * (1.0 - s.mix) + y * s.mix);   // 같은 신호의 두 버전이므로 선형 혼합
        }
    }
}
