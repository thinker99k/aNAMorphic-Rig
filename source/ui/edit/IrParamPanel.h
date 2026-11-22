#pragma once

#include "ParamPanel.h"
#include "ui/ParamKnob.h"
#include "ui/ParamSwitch.h"

namespace rig
{
    class RigModel;

    // IR 블록 선택 시: 왼쪽 절반 [LC 스위치][LC 노브], 오른쪽 절반 [RC 스위치][RC 노브]
    // 값은 선택된 블록의 현재 씬 값 (RigModel::getParam / setParam). LC 와 RC 는 스위치·흐림이 서로 독립
    class IrParamPanel final : public ParamPanel
    {
    public:
        explicit IrParamPanel (RigModel& model);

        void refresh() override;
        void resized() override;

    private:
        RigModel& model;

        ParamSwitch lowCutSwitch { "LC" }, highCutSwitch { "RC" };
        ParamKnob lowCut, highCut;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IrParamPanel)
    };
}
