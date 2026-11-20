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
        // 노브 -> 원자 변수 (돌리는 동안은 알림·저장 없음), 드래그가 끝나면 저장
        auto connect = [this] (ParamKnob& knob, std::atomic<float> P::* field)
        {
            knob.onValueChanged = [this, field] (double v) { if (params != nullptr) (params->*field).store ((float) v); };
            knob.onDragEnded = [this] { model.saveParams(); };
            addAndMakeVisible (knob);
        };

        connect (bass,       &P::bassDb);
        connect (bassFreq,   &P::bassHz);
        connect (middle,     &P::middleDb);
        connect (middleFreq, &P::middleHz);
        connect (treble,     &P::trebleDb);
        connect (trebleFreq, &P::trebleHz);

        // 스위치는 누르는 즉시 저장
        eqSwitch.onClick = [this]
        {
            if (params != nullptr)
                params->eqOn.store (eqSwitch.getToggleState());

            refresh();
            model.saveParams();
        };
        addAndMakeVisible (eqSwitch);

        refresh();
    }

    void NamParamPanel::refresh()
    {
        params = dynamic_cast<NamParams*> (model.getParams (BlockType::Nam));
        if (params == nullptr)
            return;

        bass.setValue       (params->bassDb.load());
        bassFreq.setValue   (params->bassHz.load());
        middle.setValue     (params->middleDb.load());
        middleFreq.setValue (params->middleHz.load());
        treble.setValue     (params->trebleDb.load());
        trebleFreq.setValue (params->trebleHz.load());

        const bool on = params->eqOn.load();
        eqSwitch.setToggleState (on, juce::dontSendNotification);

        for (auto* knob : { &bass, &bassFreq, &middle, &middleFreq, &treble, &trebleFreq })
            knob->setDimmed (! on);
    }

    void NamParamPanel::resized()
    {
        // [EQ 스위치 영역] [노브 6개, 같은 폭]
        auto area = getLocalBounds();
        auto switchArea = area.removeFromLeft (area.getWidth() / 10);
        eqSwitch.setBounds (switchArea.withSizeKeepingCentre (switchArea.getWidth(), getHeight() / 3));

        area.removeFromLeft (getWidth() / 60);
        const int knobWidth = area.getWidth() / 6;

        for (auto* knob : { &bass, &bassFreq, &middle, &middleFreq, &treble, &trebleFreq })
            knob->setBounds (area.removeFromLeft (knobWidth).reduced (0, getHeight() / 10));
    }
}
