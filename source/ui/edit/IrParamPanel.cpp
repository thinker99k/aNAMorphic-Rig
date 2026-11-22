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
            knob.onValueChanged = [this, index] (double v) { model.setParam (model.getSelectedSlotId(), index, (float) v, false); };
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

        for (auto* c : std::initializer_list<juce::Component*> { &lowCutPower, &lowCut, &highCutPower, &highCut })
            addAndMakeVisible (c);

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
        // [전원] 8 [노브 칸]  64  [전원] 8 [노브 칸] : 전원은 빨간 SubPowerButton (합류 갈래와 같다), 노브 칸은 NAM 노브와 같은 폭
        auto area = getLocalBounds();
        const int size = juce::jmin (SubPowerButton::size, getHeight() - 8);

        auto place = [&] (PowerButton& power, ParamKnob& knob)
        {
            power.setBounds (area.removeFromLeft (size).withSizeKeepingCentre (size, size));
            area.removeFromLeft (powerGap);
            knob.setBounds (area.removeFromLeft (knobWidth()).reduced (0, knobMargin));
        };

        place (lowCutPower, lowCut);
        area.removeFromLeft (groupGap);
        place (highCutPower, highCut);
    }
}
