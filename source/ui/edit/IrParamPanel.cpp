#include "IrParamPanel.h"

#include "engine/param/ir/IrParams.h"
#include "model/RigModel.h"

namespace rig
{
    IrParamPanel::IrParamPanel (RigModel& m)
        : model (m),
          lowCut  ("L. Cut", IrParams::lowCutMinHz,  IrParams::lowCutMaxHz,  IrParams::lowCutDefaultHz,  ParamKnob::ValueType::HzLog),
          highCut ("H. Cut", IrParams::highCutMinHz, IrParams::highCutMaxHz, IrParams::highCutDefaultHz, ParamKnob::ValueType::HzLog)
    {
        // 노브 -> 선택된 블록의 현재 씬 값 (돌리는 동안은 알림·저장 없음), 드래그가 끝나면 저장. 둘 다 시계 방향으로 값이 커진다
        auto connect = [this] (ParamKnob& knob, int index)
        {
            knob.onValueChanged = [this, index] (double value) { model.setParam (model.getSelectedSlotId(), index, (float) value, false); };
            knob.onDragEnded = [this, index, &knob] { model.setParam (model.getSelectedSlotId(), index, (float) knob.getValue()); };
        };

        connect (lowCut,  IrParams::lowCutHz);
        connect (highCut, IrParams::highCutHz);

        // 전원은 누르는 즉시 저장
        auto connectPower = [this] (PowerButton& power, int index)
        {
            power.onClick = [this, index, &power]
            {
                model.setParam (model.getSelectedSlotId(), index, power.getToggleState() ? 1.0f : 0.0f);
                refresh();
            };
        };

        connectPower (lowCutPower,  IrParams::lowCutOn);
        connectPower (highCutPower, IrParams::highCutOn);

        // 영역 묶음: [빨간 전원][노브]
        auto group = [this] (ParamGroup& g, SubPowerButton& power, ParamKnob& knob)
        {
            g.addItem (power, SubPowerButton::size, 0, SubPowerButton::size);
            g.addItem (knob, knob.getIdealWidth(), ParamGroup::powerGap);
            addAndMakeVisible (g);
        };

        group (lowCutGroup,  lowCutPower,  lowCut);
        group (highCutGroup, highCutPower, highCut);

        refresh();
    }

    void IrParamPanel::refresh()
    {
        const auto* slot = model.getSelectedSlot();
        if (slot == nullptr || slot->type != BlockType::Ir)
            return;

        lowCut.setValue (model.getParam (*slot, IrParams::lowCutHz));
        highCut.setValue (model.getParam (*slot, IrParams::highCutHz));

        const bool lowOn = model.getParam (*slot, IrParams::lowCutOn) >= 0.5f;
        const bool highOn = model.getParam (*slot, IrParams::highCutOn) >= 0.5f;

        lowCutPower.setToggleState (lowOn, juce::dontSendNotification);
        highCutPower.setToggleState (highOn, juce::dontSendNotification);
        lowCut.setEnabled (lowOn);
        highCut.setEnabled (highOn);
    }

    void IrParamPanel::resized()
    {
        layoutGroups ({ { &lowCutGroup, 2, 3 }, { &highCutGroup, 5, 3 } });   // 2 ~ 4, 5 ~ 7 칸
    }
}
