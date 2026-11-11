#pragma once

#include "engine/AudioEngine.h"
#include "model/RigModel.h"
#include "model/NamMetadata.h"

#include <map>

#include <juce_gui_basics/juce_gui_basics.h>

namespace rig
{
    // 선택된 블록 설정 (화면 하단 절반 전체).
    // 파일 목록에서 .nam / .wav 를 고른다.
    //   - 파일 목록 = ~/Documents/aNAMorphic-Rig/nam (*.nam) 또는 ~/Documents/aNAMorphic-Rig/ir (*.wav)
    class BlockEditor final : public juce::Component,
                              private juce::ChangeListener,
                              private juce::TableListBoxModel
    {
    public:
        BlockEditor (RigModel& model, AudioEngine& engine);
        ~BlockEditor() override;

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        void changeListenerCallback (juce::ChangeBroadcaster*) override;
        void refresh();
        int getNumRows() override;
        void paintRowBackground (juce::Graphics&, int row, int width, int height, bool selected) override;
        void paintCell (juce::Graphics&, int row, int columnId, int width, int height, bool selected) override;
        void selectedRowsChanged (int row) override;

        RigModel& model;
        AudioEngine& engine;

        juce::Label title;
        juce::TableListBox fileList;
        juce::Label folderHint;
        juce::Label status;

        juce::Array<juce::File> fileForItem;   // 목록 행 -> 파일
        juce::StringArray fileNames;
        juce::StringArray displayNames;
        juce::StringArray gearTypes;
        struct CachedMetadata
        {
            juce::Time modified;
            juce::int64 size = -1;
            NamMetadata metadata;
        };
        std::map<juce::String, CachedMetadata> metadataCache;
        bool refreshing = false;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BlockEditor)
    };
}
