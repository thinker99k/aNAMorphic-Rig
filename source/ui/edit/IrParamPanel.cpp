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
        // 노브 -> 원자 변수 (돌리는 동안은 알림·저장 없음), 드래그가 끝나면 저장. 둘 다 시계 방향으로 값이 커진다
        lowCut.onValueChanged  = [this] (double v) { if (params != nullptr) params->lowCutHz.store ((float) v); };
        highCut.onValueChanged = [this] (double v) { if (params != nullptr) params->highCutHz.store ((float) v); };
        lowCut.onDragEnded  = [this] { model.saveParams(); };
        highCut.onDragEnded = [this] { model.saveParams(); };

        // 스위치는 누르는 즉시 저장
        lowCutSwitch.onClick = [this]
        {
            if (params != nullptr)
                params->lowCutOn.store (lowCutSwitch.getToggleState());

            refresh();
            model.saveParams();
        };

        highCutSwitch.onClick = [this]
        {
            if (params != nullptr)
                params->highCutOn.store (highCutSwitch.getToggleState());

            refresh();
            model.saveParams();
        };

        for (auto* c : std::initializer_list<juce::Component*> { &lowCutSwitch, &lowCut, &highCutSwitch, &highCut })
            addAndMakeVisible (c);

        refresh();
    }

    void IrParamPanel::refresh()
    {
        params = dynamic_cast<IrParams*> (model.getParams (BlockType::Ir));
        if (params == nullptr)
            return;

        lowCut.setValue (params->lowCutHz.load());
        highCut.setValue (params->highCutHz.load());

        const bool lowOn = params->lowCutOn.load();
        const bool highOn = params->highCutOn.load();

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
