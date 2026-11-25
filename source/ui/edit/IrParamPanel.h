#pragma once

#include "ParamPanel.h"
#include "ui/ParamKnob.h"

namespace rig
{
    class RigModel;

    // IR 블록 선택 시: 두 칸마다 왼쪽 끝에 [전원][노브] 한 쌍, L. Cut 2 ~ 3 칸, H. Cut 7 ~ 8 칸 (Low / High Cut)
    // 값은 선택된 블록의 현재 씬 값 (RigModel::getParam / setParam). 전원이 꺼진 쪽 노브는 흐리고 조작 안 됨
    class IrParamPanel final : public ParamPanel
    {
    public:
        explicit IrParamPanel (RigModel& model);

        void refresh() override;
        juce::Component* componentForParam (int index) override;
        void resized() override;

    private:
        RigModel& model;

        SubPowerButton lowCutPower, highCutPower;   // 빨간 전원 (초록은 블록 켜기 전용)
        ParamKnob lowCut, highCut;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IrParamPanel)
    };
}
