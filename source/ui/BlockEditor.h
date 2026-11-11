#pragma once

#include "engine/AudioEngine.h"
#include "library/NamMetadata.h"
#include "model/RigModel.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace rig
{
    // 선택된 블록 설정 (화면 하단 절반 전체).
    // 파일 목록에서 .nam / .wav 를 고른다 (더블클릭해야 블록에 지정된다. 클릭/방향키는 강조만).
    //   - 파일 목록 = ~/Documents/aNAMorphic-Rig/nam (*.nam) 또는 ~/Documents/aNAMorphic-Rig/ir (*.wav)
    //   - 폴더 스캔은 refresh() 에서만 한다 (생성 시 한 번). 모델 변경 알림 때는 캐시에서 목록만 다시 고른다
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
        struct Entry
        {
            juce::File file;
            juce::String path;          // Path 열: 폴더 기준 상대 경로 (폴더 밖 파일은 전체 경로)
            juce::String displayName;   // NAM 은 metadata.name, 없으면 파일명
            juce::String gearType;
        };

        static Entry makeEntry (const juce::File& file, const juce::String& path, bool readMetadata);

        void changeListenerCallback (juce::ChangeBroadcaster*) override;
        void refresh();
        void updateFromModel();
        int getNumRows() override;
        void paintRowBackground (juce::Graphics&, int row, int width, int height, bool selected) override;
        void paintCell (juce::Graphics&, int row, int columnId, int width, int height, bool selected) override;
        void cellDoubleClicked (int row, int columnId, const juce::MouseEvent&) override;
        void showFileNotFound (const juce::File& file);

        RigModel& model;
        AudioEngine& engine;

        juce::Label title;
        juce::TableListBox fileList;
        juce::Label folderHint;
        juce::Label status;

        std::vector<Entry> namEntries, irEntries;   // refresh() 의 스캔 결과
        Entry outsideEntry;                         // 폴더 밖의 현재 파일 (메타데이터를 매번 읽지 않도록 보관)

        std::vector<Entry> rows;                    // 지금 보이는 목록

        // 강조를 현재 파일로 되돌리는 건 선택 블록이나 그 파일이 바뀌었을 때만
        juce::uint64 shownSlotId = 0;
        juce::File shownFile;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BlockEditor)
    };
}
