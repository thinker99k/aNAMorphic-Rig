#pragma once

#include "ParamPanel.h"
#include "ui/ParamKnob.h"
#include "ui/ParamSwitch.h"

namespace rig
{
    class NamParams;
    class RigModel;

    // NAM 블록 선택 시: [EQ 스위치] [Bass] [Bass Freq] [Middle] [Middle Freq] [Treble] [Treble Freq]
    // 값은 선택된 블록의 NamParams. 스위치가 꺼져 있으면 노브는 흐리지만 조작은 된다
    class NamParamPanel final : public ParamPanel
    {
    public:
        explicit NamParamPanel (RigModel& model);

        void refresh() override;
        void resized() override;

    private:
        RigModel& model;
        std::shared_ptr<NamParams> params;   // refresh() 마다 선택된 블록의 것으로 다시 얻는다

        ParamSwitch eqSwitch { "EQ" };
        ParamKnob bass, bassFreq, middle, middleFreq, treble, trebleFreq;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NamParamPanel)
    };
}
