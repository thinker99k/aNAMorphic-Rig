#pragma once

#include "ParamPanel.h"
#include "ui/ParamKnob.h"

namespace rig
{
    class RigModel;

    // NAM 블록 선택 시: 영역 묶음 3개 [B][B. Freq]  [M][M. Freq][Q]  [T][T. Freq] (Bass / Middle / Treble, 왼쪽부터)
    // 값은 선택된 블록의 현재 씬 값 (RigModel::getParam / setParam). EQ 는 항상 켜져 있다
    class NamParamPanel final : public ParamPanel
    {
    public:
        explicit NamParamPanel (RigModel& model);

        void refresh() override;
        void resized() override;

    private:
        RigModel& model;

        ParamKnob bass, bassFreq, middle, middleFreq, middleQ, treble, trebleFreq;
        ParamGroup bassGroup, middleGroup, trebleGroup;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NamParamPanel)
    };
}
