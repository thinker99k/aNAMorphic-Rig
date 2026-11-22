#pragma once

#include "ParamPanel.h"
#include "ui/ParamKnob.h"

namespace rig
{
    class RigModel;

    // NAM 블록 선택 시: [B][B. Freq]  [M][M. Freq]  [T][T. Freq] (Bass / Middle / Treble, 왼쪽부터. 같은 영역끼리 가깝게)
    // 값은 선택된 블록의 현재 씬 값 (RigModel::getParam / setParam). EQ 는 항상 켜져 있다
    class NamParamPanel final : public ParamPanel
    {
    public:
        explicit NamParamPanel (RigModel& model);

        void refresh() override;
        void resized() override;

    private:
        RigModel& model;

        ParamKnob bass, bassFreq, middle, middleFreq, treble, trebleFreq;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NamParamPanel)
    };
}
