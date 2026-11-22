#include "NamParamPanel.h"

#include "engine/param/nam/NamParams.h"
#include "model/RigModel.h"

namespace rig
{
    using P = NamParams;
    using VT = ParamKnob::ValueType;

    NamParamPanel::NamParamPanel (RigModel& m)
        : model (m),
          bass       ("Bass",        P::gainMinDb,   P::gainMaxDb,   VT::Db),
          bassFreq   ("Bass Freq",   P::bassMinHz,   P::bassMaxHz,   VT::HzLog),
          middle     ("Middle",      P::gainMinDb,   P::gainMaxDb,   VT::Db),
          middleFreq ("Middle Freq", P::middleMinHz, P::middleMaxHz, VT::HzLog),
          treble     ("Treble",      P::gainMinDb,   P::gainMaxDb,   VT::Db),
          trebleFreq ("Treble Freq", P::trebleMinHz, P::trebleMaxHz, VT::HzLog)
    {
        // 노브 -> 선택된 블록의 현재 씬 값 (돌리는 동안은 알림·저장 없음), 드래그가 끝나면 저장
        auto connect = [this] (ParamKnob& knob, int index)
        {
            knob.onValueChanged = [this, index] (double v) { model.setParam (model.getSelectedSlotId(), index, (float) v, false); };
            knob.onDragEnded = [this, index, &knob] { model.setParam (model.getSelectedSlotId(), index, (float) knob.getValue()); };
            addAndMakeVisible (knob);
        };

        connect (bass,       P::bassDb);
        connect (bassFreq,   P::bassHz);
        connect (middle,     P::middleDb);
        connect (middleFreq, P::middleHz);
        connect (treble,     P::trebleDb);
        connect (trebleFreq, P::trebleHz);

        refresh();
    }

    void NamParamPanel::refresh()
    {
        const auto* slot = model.getSelectedSlot();
        if (slot == nullptr || slot->type != BlockType::Nam)
            return;

        bass.setValue       (model.getParam (*slot, P::bassDb));
        bassFreq.setValue   (model.getParam (*slot, P::bassHz));
        middle.setValue     (model.getParam (*slot, P::middleDb));
        middleFreq.setValue (model.getParam (*slot, P::middleHz));
        treble.setValue     (model.getParam (*slot, P::trebleDb));
        trebleFreq.setValue (model.getParam (*slot, P::trebleHz));
    }

    void NamParamPanel::resized()
    {
        // 노브 6개, 왼쪽부터 같은 폭
        auto area = getLocalBounds().reduced (0, knobMargin);

        for (auto* knob : { &bass, &bassFreq, &middle, &middleFreq, &treble, &trebleFreq })
            knob->setBounds (area.removeFromLeft (knobWidth()));
    }
}
