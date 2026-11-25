#pragma once

#include "ParamPanel.h"
#include "ui/ParamKnob.h"

namespace rig
{
    class RigModel;

    // NAM 블록 선택 시: 3 ~ 9 칸에 노브 하나씩 [Bass][B. Freq][Mid][M. Freq][Q][Treble][T. Freq]
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

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NamParamPanel)
    };
}
