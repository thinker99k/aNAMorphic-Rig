#pragma once

#include "library/NamMetadata.h"
#include "model/RigModel.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace rig
{
    // 선택된 블록 설정 (화면 하단 절반 전체).
    // 파일 목록에서 .nam / .wav 를 고른다 (더블클릭해야 블록에 지정된다. 클릭/방향키는 강조만).
    //   - 파일 목록 = ~/Documents/aNAMorphic-Rig/nam (*.nam) 또는 ~/Documents/aNAMorphic-Rig/ir (*.wav)
    //   - 폴더 스캔은 refresh() 에서만 한다 (생성 시 한 번). 모델 변경 알림 때는 캐시에서 목록만 다시 고른다
    //   - NAM / IR 표를 따로 두어 열 폭과 정렬 상태를 각자 유지한다 (메모리에만, 다시 켜면 기본값)
    class BlockEditor final : public juce::Component,
                              private juce::ChangeListener,
                              private juce::TableListBoxModel
    {
    public:
        explicit BlockEditor (RigModel& model);
        ~BlockEditor() override;

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        enum Column { nameColumn = 1, gearModelColumn, gearTypeColumn, toneColumn, modeledByColumn };

        struct Entry
        {
            juce::File file;
            juce::String displayName;   // NAM 은 metadata.name, 없으면 파일명
            juce::String gearModel, gearType, tone, modeledBy;

            const juce::String& get (int columnId) const;
        };

        static Entry makeEntry (const juce::File& file, bool readMetadata);

        void changeListenerCallback (juce::ChangeBroadcaster*) override;
        void refresh();
        void updateFromModel();
        int getNumRows() override;
        void paintRowBackground (juce::Graphics&, int row, int width, int height, bool selected) override;
        void paintCell (juce::Graphics&, int row, int columnId, int width, int height, bool selected) override;
        void cellDoubleClicked (int row, int columnId, const juce::MouseEvent&) override;
        void sortOrderChanged (int columnId, bool isForwards) override;
        void sortRows();
        void showFileNotFound (const juce::File& file);

        juce::TableListBox& activeList() { return rowsType == BlockType::Nam ? namList : irList; }

        // 마지막 열 오른쪽 경계선만 그리지 않아, 스크롤바 위 머리글 빈칸과 이어져 보이게 한다
        struct HeaderLookAndFeel final : juce::LookAndFeel_V4
        {
            void drawTableHeaderBackground (juce::Graphics&, juce::TableHeaderComponent&) override;
        };

        RigModel& model;

        juce::Label title;
        HeaderLookAndFeel headerLookAndFeel;   // 표보다 먼저 선언: 표가 먼저 소멸한다
        juce::TableListBox namList, irList;

        std::vector<Entry> namEntries, irEntries;   // refresh() 의 스캔 결과
        Entry outsideEntry;                         // 폴더 밖의 현재 파일 (메타데이터를 매번 읽지 않도록 보관)

        std::vector<Entry> rows;                    // 지금 보이는 목록 (정렬된 상태)
        BlockType rowsType = BlockType::Nam;        // rows 가 어떤 블록 종류의 목록인지

        // 강조를 현재 파일로 되돌리는 건 선택 블록이나 그 파일이 바뀌었을 때만
        juce::uint64 shownSlotId = 0;
        juce::File shownFile;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BlockEditor)
    };
}
