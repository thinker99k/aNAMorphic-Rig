#include "IrParamPanel.h"

#include "engine/param/ir/IrParams.h"
#include "model/RigModel.h"

namespace rig
{
    IrParamPanel::IrParamPanel (RigModel& m)
        : model (m),
          lowCut  ("LC", IrParams::lowCutMinHz,  IrParams::lowCutMaxHz,  ParamKnob::ValueType::HzLog),
          highCut ("RC", IrParams::highCutMinHz, IrParams::highCutMaxHz, ParamKnob::ValueType::HzLog)
    {
        // 노브 -> 선택된 블록의 현재 씬 값 (돌리는 동안은 알림·저장 없음), 드래그가 끝나면 저장. 둘 다 시계 방향으로 값이 커진다
        auto connect = [this] (ParamKnob& knob, int index)
        {
            knob.onValueChanged = [this, index] (double v) { model.setParam (model.getSelectedSlotId(), index, (float) v, false); };
            knob.onDragEnded = [this, index, &knob] { model.setParam (model.getSelectedSlotId(), index, (float) knob.getValue()); };
        };

        connect (lowCut,  IrParams::lowCutHz);
        connect (highCut, IrParams::highCutHz);

        // 스위치는 누르는 즉시 저장
        auto connectSwitch = [this] (ParamSwitch& sw, int index)
        {
            sw.onClick = [this, index, &sw]
            {
                model.setParam (model.getSelectedSlotId(), index, sw.getToggleState() ? 1.0f : 0.0f);
                refresh();
            };
        };

        connectSwitch (lowCutSwitch,  IrParams::lowCutOn);
        connectSwitch (highCutSwitch, IrParams::highCutOn);

        for (auto* c : std::initializer_list<juce::Component*> { &lowCutSwitch, &lowCut, &highCutSwitch, &highCut })
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

        lowCutSwitch.setToggleState (lowOn, juce::dontSendNotification);
        highCutSwitch.setToggleState (highOn, juce::dontSendNotification);
        lowCut.setDimmed (! lowOn);
        highCut.setDimmed (! highOn);
    }

    void IrParamPanel::resized()
    {
        // 좌우 절반: [스위치][노브] | [스위치][노브]
        auto area = getLocalBounds();
        auto left = area.removeFromLeft (area.getWidth() / 2);
        auto right = area;

        auto place = [this] (juce::Rectangle<int> half, ParamSwitch& sw, ParamKnob& knob)
        {
            auto switchArea = half.removeFromLeft (getWidth() / 10);
            sw.setBounds (switchArea.withSizeKeepingCentre (switchArea.getWidth(), getHeight() / 3));
            half.removeFromLeft (getWidth() / 60);
            knob.setBounds (half.removeFromLeft (getWidth() / 6).reduced (0, getHeight() / 10));
        };

        place (left, lowCutSwitch, lowCut);
        place (right, highCutSwitch, highCut);
    }
}
