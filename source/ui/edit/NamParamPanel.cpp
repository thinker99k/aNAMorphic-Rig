#include "NamParamPanel.h"

#include "engine/param/nam/NamParams.h"
#include "model/RigModel.h"

namespace rig
{
    using P = NamParams;
    using VT = ParamKnob::ValueType;

    namespace
    {
        constexpr int firstCell = 3;   // 첫 노브(Bass)의 칸
    }

    NamParamPanel::NamParamPanel (RigModel& m)
        : model (m),
          bass       ("Bass",    P::gainMinDb,   P::gainMaxDb,   P::gainDefaultDb,   VT::Db),
          bassFreq   ("B. Freq", P::bassMinHz,   P::bassMaxHz,   P::bassDefaultHz,   VT::HzLog),
          middle     ("Mid",     P::gainMinDb,   P::gainMaxDb,   P::gainDefaultDb,   VT::Db),
          middleFreq ("M. Freq", P::middleMinHz, P::middleMaxHz, P::middleDefaultHz, VT::HzLog),
          middleQ    ("Q",       P::middleQMin,  P::middleQMax,  P::middleQDefault,  VT::QLog),
          treble     ("Treble",  P::gainMinDb,   P::gainMaxDb,   P::gainDefaultDb,   VT::Db),
          trebleFreq ("T. Freq", P::trebleMinHz, P::trebleMaxHz, P::trebleDefaultHz, VT::HzLog)
    {
        // 노브 -> 선택된 블록의 현재 씬 값 (돌리는 동안은 알림·저장 없음), 드래그가 끝나면 저장
        auto connect = [this] (ParamKnob& knob, int index)
        {
            knob.onValueChanged = [this, index] (double value) { model.setParam (model.getSelectedSlotId(), index, (float) value, false); };
            knob.onDragEnded = [this, index, &knob] { model.setParam (model.getSelectedSlotId(), index, (float) knob.getValue()); };
        };

        connect (bass,       P::bassDb);
        connect (bassFreq,   P::bassHz);
        connect (middle,     P::middleDb);
        connect (middleFreq, P::middleHz);
        connect (middleQ,    P::middleQ);
        connect (treble,     P::trebleDb);
        connect (trebleFreq, P::trebleHz);

        for (auto* knob : { &bass, &bassFreq, &middle, &middleFreq, &middleQ, &treble, &trebleFreq })
            addAndMakeVisible (knob);

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
        middleQ.setValue    (model.getParam (*slot, P::middleQ));
        treble.setValue     (model.getParam (*slot, P::trebleDb));
        trebleFreq.setValue (model.getParam (*slot, P::trebleHz));
    }

    void NamParamPanel::resized()
    {
        // 3 ~ 9 칸에 하나씩, 칸 가운데 (노브 폭 = 칸 폭, ParamKnob 이 노브와 이름을 가운데에 놓는다)
        int cell = firstCell;

        for (auto* knob : { &bass, &bassFreq, &middle, &middleFreq, &middleQ, &treble, &trebleFreq })
            knob->setBounds (bottomBarCell (getLocalBounds(), cell++));
    }
}
