#include "NamParamPanel.h"

#include "engine/param/nam/NamParams.h"
#include "model/RigModel.h"

namespace rig
{
    using P = NamParams;
    using VT = ParamKnob::ValueType;

    NamParamPanel::NamParamPanel (RigModel& m)
        : model (m),
          bass       ("B",           P::gainMinDb,   P::gainMaxDb,   P::gainDefaultDb,   VT::Db),
          bassFreq   ("B. Freq",     P::bassMinHz,   P::bassMaxHz,   P::bassDefaultHz,   VT::HzLog),
          middle     ("M",           P::gainMinDb,   P::gainMaxDb,   P::gainDefaultDb,   VT::Db),
          middleFreq ("M. Freq",     P::middleMinHz, P::middleMaxHz, P::middleDefaultHz, VT::HzLog),
          middleQ    ("Q",           P::middleQMin,  P::middleQMax,  P::middleQDefault,  VT::QLog),
          treble     ("T",           P::gainMinDb,   P::gainMaxDb,   P::gainDefaultDb,   VT::Db),
          trebleFreq ("T. Freq",     P::trebleMinHz, P::trebleMaxHz, P::trebleDefaultHz, VT::HzLog)
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

        // 영역 묶음: [세기][주파수]
        auto group = [this] (ParamGroup& g, ParamKnob& gain, ParamKnob& freq)
        {
            g.addItem (gain, gain.getIdealWidth(), 0);
            g.addItem (freq, freq.getIdealWidth(), ParamGroup::knobGap);
            addAndMakeVisible (g);
        };

        group (bassGroup,   bass,   bassFreq);
        group (middleGroup, middle, middleFreq);
        middleQ.setDiameter (ParamKnob::smallKnobDiameter);   // Middle 만 있는 보조 노브: 조금 작게
        middleGroup.addItem (middleQ, middleQ.getIdealWidth(), ParamGroup::knobGap);
        group (trebleGroup, treble, trebleFreq);

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
        layoutGroups ({ { &bassGroup, 2, 3 }, { &middleGroup, 5, 4 }, { &trebleGroup, 9, 3 } });   // 2 ~ 4, 5 ~ 8, 9 ~ 11 칸
    }
}
