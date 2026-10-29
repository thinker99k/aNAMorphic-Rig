#pragma once

#include "engine/AudioEngine.h"
#include "model/RigModel.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace rig
{
    // 선택된 블록 설정 (화면 하단 절반의 위쪽 절반).
    // 지금은 파일 지정만: 콤보박스에서 .nam / .wav 를 고른다.
    //   - 콤보 목록 = ~/aNAMorphic-Rig/captures (*.nam) 또는 ~/aNAMorphic-Rig/irs (*.wav)
    //   - 맨 아래 "Browse..." 로 다른 위치의 파일도 지정 가능
    class BlockEditor final : public juce::Component,
                              private juce::ChangeListener
    {
    public:
        BlockEditor (RigModel& model, AudioEngine& engine);
        ~BlockEditor() override;

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        void changeListenerCallback (juce::ChangeBroadcaster*) override;
        void refresh();
        void fileChosen();
        void browse();

        RigModel& model;
        AudioEngine& engine;

        juce::Label title;
        juce::Label fileLabel { {}, "File" };
        juce::ComboBox fileBox;
        juce::Label folderHint;
        juce::Label status;

        juce::Array<juce::File> fileForItem;   // 콤보 항목 id-1 -> 파일
        std::unique_ptr<juce::FileChooser> chooser;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BlockEditor)
    };
}
