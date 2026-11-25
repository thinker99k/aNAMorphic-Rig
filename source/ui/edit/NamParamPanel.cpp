#include "NamParamPanel.h"

#include "engine/param/nam/NamParams.h"
#include "model/RigModel.h"

namespace rig
{
    using P = NamParams;
    using VT = ParamKnob::ValueType;

    namespace
    {
        constexpr int firstCell = 2;                        // 첫 노브(Gain)의 칸
        constexpr int separatorsAfter[] = { 2, 9 };         // 이 칸 뒤에 구분선: Gain | EQ | Presence·Resonance
    }

    NamParamPanel::NamParamPanel (RigModel& m)
        : model (m),
          gain       ("Gain",      P::gainMinDb,   P::gainMaxDb,   P::gainDefaultDb,   VT::Db),
          bass       ("Bass",      P::gainMinDb,   P::gainMaxDb,   P::gainDefaultDb,   VT::Db),
          bassFreq   ("B. Freq",   P::bassMinHz,   P::bassMaxHz,   P::bassDefaultHz,   VT::HzLog),
          middle     ("Mid",       P::gainMinDb,   P::gainMaxDb,   P::gainDefaultDb,   VT::Db),
          middleFreq ("M. Freq",   P::middleMinHz, P::middleMaxHz, P::middleDefaultHz, VT::HzLog),
          middleQ    ("Q",         P::middleQMin,  P::middleQMax,  P::middleQDefault,  VT::QLog),
          treble     ("Treble",    P::gainMinDb,   P::gainMaxDb,   P::gainDefaultDb,   VT::Db),
          trebleFreq ("T. Freq",   P::trebleMinHz, P::trebleMaxHz, P::trebleDefaultHz, VT::HzLog),
          presence   ("Presence",  P::gainMinDb,   P::gainMaxDb,   P::gainDefaultDb,   VT::Db),
          resonance  ("Resonance", P::gainMinDb,   P::gainMaxDb,   P::gainDefaultDb,   VT::Db),
          items { { { gain, P::gainDb }, { bass, P::bassDb }, { bassFreq, P::bassHz }, { middle, P::middleDb }, { middleFreq, P::middleHz },
                    { middleQ, P::middleQ }, { treble, P::trebleDb }, { trebleFreq, P::trebleHz },
                    { presence, P::presenceDb }, { resonance, P::resonanceDb } } }
    {
        // 노브 -> 선택된 블록의 현재 씬 값 (돌리는 동안은 알림·저장 없음), 드래그가 끝나면 저장
        for (const auto& item : items)
        {
            auto& knob = item.knob;
            const int index = item.index;
            knob.onValueChanged = [this, index] (double value) { model.setParam (model.getSelectedSlotId(), index, (float) value, false); };
            knob.onDragEnded = [this, index, &knob] { model.setParam (model.getSelectedSlotId(), index, (float) knob.getValue()); };
            addAndMakeVisible (knob);
        }

        refresh();
    }

    void NamParamPanel::refresh()
    {
        const auto* slot = model.getSelectedSlot();
        if (slot == nullptr || slot->type != BlockType::Nam)
            return;

        for (const auto& item : items)
            item.knob.setValue (model.getParam (*slot, item.index));
    }

    juce::Component* NamParamPanel::componentForParam (int index)
    {
        for (const auto& item : items)
            if (item.index == index)
                return &item.knob;

        return nullptr;
    }

    void NamParamPanel::paint (juce::Graphics& g)
    {
        for (const int cell : separatorsAfter)
            drawCellSeparator (g, getLocalBounds(), cell);
    }

    void NamParamPanel::resized()
    {
        // 2 ~ 11 칸에 하나씩, 칸 가운데 (노브 폭 = 칸 폭, ParamKnob 이 노브와 이름을 가운데에 놓는다)
        int cell = firstCell;

        for (const auto& item : items)
            item.knob.setBounds (bottomBarCell (getLocalBounds(), cell++));
    }
}
