#include "EqStage.h"

namespace rig
{
    namespace
    {
        constexpr double rampSeconds = transitionSeconds;   // 값 변경 20 ms (Block.h)
    }

    EqStage::EqStage (std::shared_ptr<BlockParams> p) : params (std::move (p)) {}

    void EqStage::prepare (double sr, int)
    {
        sampleRate = sr;

        for (auto* s : { &bassDb, &middleDb, &trebleDb })
            s->reset (sr, rampSeconds);
        for (auto* s : { &bassHz, &middleHz, &trebleHz, &middleQ })
            s->reset (sr, rampSeconds);

        jumpToTargets();
        low.reset(); middle.reset(); high.reset();
    }

    void EqStage::jumpToTargets() noexcept
    {
        bassDb.setCurrentAndTargetValue   (params->get (NamParams::bassDb));
        middleDb.setCurrentAndTargetValue (params->get (NamParams::middleDb));
        trebleDb.setCurrentAndTargetValue (params->get (NamParams::trebleDb));
        bassHz.setCurrentAndTargetValue   (params->get (NamParams::bassHz));
        middleHz.setCurrentAndTargetValue (params->get (NamParams::middleHz));
        trebleHz.setCurrentAndTargetValue (params->get (NamParams::trebleHz));
        middleQ.setCurrentAndTargetValue  (params->get (NamParams::middleQ));
    }

    void EqStage::setTargets() noexcept
    {
        bassDb.setTargetValue   (params->get (NamParams::bassDb));
        middleDb.setTargetValue (params->get (NamParams::middleDb));
        trebleDb.setTargetValue (params->get (NamParams::trebleDb));
        bassHz.setTargetValue   (params->get (NamParams::bassHz));
        middleHz.setTargetValue (params->get (NamParams::middleHz));
        trebleHz.setTargetValue (params->get (NamParams::trebleHz));
        middleQ.setTargetValue  (params->get (NamParams::middleQ));
    }

    void EqStage::process (float* io, int n) noexcept
    {
        // 값은 구간 단위로 따라가고 계수는 구간마다 한 번 계산한다
        setTargets();
        low.setPeak    (sampleRate, bassHz.skip (n),   NamParams::eqQ, bassDb.skip (n));
        middle.setPeak (sampleRate, middleHz.skip (n), middleQ.skip (n), middleDb.skip (n));
        high.setPeak   (sampleRate, trebleHz.skip (n), NamParams::eqQ, trebleDb.skip (n));

        for (int i = 0; i < n; ++i)
            io[i] = (float) high.process (middle.process (low.process ((double) io[i])));
    }
}
