#pragma once

#include "ParamPanel.h"
#include "ui/ParamKnob.h"

namespace rig
{
    class RigModel;

    // IR 블록 선택 시: [L. Cut 전원][L. Cut 노브]  [H. Cut 전원][H. Cut 노브] (Low / High Cut, 왼쪽부터)
    // 값은 선택된 블록의 현재 씬 값 (RigModel::getParam / setParam). 전원이 꺼진 쪽 노브는 흐리고 조작 안 됨
    class IrParamPanel final : public ParamPanel
    {
    public:
        explicit IrParamPanel (RigModel& model);

        void refresh() override;
        void resized() override;

    private:
        RigModel& model;

        PowerButton lowCutPower, highCutPower;
        ParamKnob lowCut, highCut;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IrParamPanel)
    };
}
