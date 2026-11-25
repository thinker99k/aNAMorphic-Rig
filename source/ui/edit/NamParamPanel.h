#pragma once

#include "ParamPanel.h"
#include "ui/ParamKnob.h"

#include <array>

namespace rig
{
    class RigModel;

    // NAM 블록 선택 시: 2 ~ 11 칸에 노브 하나씩, 묶음 사이에 구분선 |
    //   [Gain] | [Bass][B. Freq][Mid][M. Freq][Q][Treble][T. Freq] | [Presence][Resonance]   (12 칸 공용 Output 과 그 앞 | 는 ParamBar)
    // 값은 선택된 블록의 현재 씬 값 (RigModel::getParam / setParam). 모두 항상 켜져 있다
    class NamParamPanel final : public ParamPanel
    {
    public:
        explicit NamParamPanel (RigModel& model);

        void refresh() override;
        juce::Component* componentForParam (int index) override;
        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        RigModel& model;

        ParamKnob gain, bass, bassFreq, middle, middleFreq, middleQ, treble, trebleFreq, presence, resonance;

        // 왼쪽부터 칸 순서 (firstCell 부터 한 칸씩), 각 노브의 파라미터 번호
        struct Item { ParamKnob& knob; int index; };
        const std::array<Item, 10> items;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NamParamPanel)
    };
}
