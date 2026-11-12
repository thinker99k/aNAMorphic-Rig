#pragma once

#include "library/NamMetadata.h"
#include "model/RigModel.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace rig
{
    // 블록 선택기 (화면 중간). 모든 블록이 함께 쓰며, 선택된 블록에 장치를 지정한다.
    //   왼쪽 = 카테고리 탭 (Empty / NAM / IR), 오른쪽 = 그 카테고리의 목록
    //   - 더블클릭해야 블록에 지정된다 (이때 패치도 저장). 탭 클릭/클릭/방향키는 둘러보기(강조)만
    //   - 파일 카테고리(NAM / IR) : ~/Documents/aNAMorphic-Rig/nam (*.nam) 또는 ir (*.wav) 의 파일 표
    //       - 폴더 스캔은 refresh() 에서만 한다 (생성 시 한 번). 모델 변경 알림 때는 캐시에서 목록만 다시 고른다
    //       - NAM / IR 표를 따로 두어 열 폭과 정렬 상태를 각자 유지한다 (메모리에만, 다시 켜면 기본값)
    //   - 파일 없는 카테고리(Empty 등) : 장치 목록. 새 카테고리는 categories 에 탭 이름과 장치 목록만 추가하면 된다
    //   - 선택된 블록이 없으면 (체인이 비었을 때) 보이기만 하고 비활성
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

        // 파일 없는 카테고리의 목록 한 줄 (더블클릭하면 블록이 이 종류가 된다)
        struct Device
        {
            juce::String name;
            BlockType type;
        };

        struct Category
        {
            juce::String name;
            bool usesFiles;                 // true 면 fileType 의 파일 표, false 면 devices 목록
            BlockType fileType;
            std::vector<Device> devices;
        };

        static Entry makeEntry (const juce::File& file, bool readMetadata);

        void changeListenerCallback (juce::ChangeBroadcaster*) override;
        void refresh();
        void updateFromModel();
        void showCategory (bool resetHighlight);
        int categoryFor (BlockType type) const;
        const Category& shownCategory() const { return categories[(size_t) shownCategoryIndex]; }

        // TableListBoxModel (NAM / IR 표)
        int getNumRows() override;
        void paintRowBackground (juce::Graphics&, int row, int width, int height, bool selected) override;
        void paintCell (juce::Graphics&, int row, int columnId, int width, int height, bool selected) override;
        void cellDoubleClicked (int row, int columnId, const juce::MouseEvent&) override;
        void sortOrderChanged (int columnId, bool isForwards) override;
        void sortRows();
        void showFileNotFound (const juce::File& file);

        // 장치 목록 (getNumRows 가 표 모델과 겹치므로 따로 둔다)
        struct DeviceListModel final : juce::ListBoxModel
        {
            explicit DeviceListModel (BlockEditor& e) : owner (e) {}

            int getNumRows() override;
            void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
            void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;

            BlockEditor& owner;
        };

        juce::TableListBox& activeList() { return shownCategory().fileType == BlockType::Ir ? irList : namList; }

        // 마지막 열 오른쪽 경계선만 그리지 않아, 스크롤바 위 머리글 빈칸과 이어져 보이게 한다
        struct HeaderLookAndFeel final : juce::LookAndFeel_V4
        {
            void drawTableHeaderBackground (juce::Graphics&, juce::TableHeaderComponent&) override;
        };

        RigModel& model;

        std::vector<Category> categories;
        juce::OwnedArray<juce::TextButton> tabs;   // categories 와 같은 순서

        HeaderLookAndFeel headerLookAndFeel;   // 표보다 먼저 선언: 표가 먼저 소멸한다
        juce::TableListBox namList, irList;
        DeviceListModel deviceListModel { *this };
        juce::ListBox deviceList;

        std::vector<Entry> namEntries, irEntries;   // refresh() 의 스캔 결과
        Entry outsideEntry;                         // 폴더 밖의 현재 파일 (메타데이터를 매번 읽지 않도록 보관)

        std::vector<Entry> rows;                    // 지금 보이는 파일 표 목록 (정렬된 상태)

        // 둘러보는 탭과 강조를 선택 블록으로 되돌리는 건 선택 블록이나 그 종류/파일이 바뀌었을 때만
        int shownCategoryIndex = 0;
        juce::uint64 shownSlotId = 0;
        BlockType shownType = BlockType::None;
        juce::File shownFile;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BlockEditor)
    };
}
