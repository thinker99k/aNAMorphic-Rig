#include "BlockEditor.h"

#include "AppPaths.h"
#include "ui/Widgets.h"

namespace rig
{
    namespace
    {
        juce::String patternFor (BlockType t) { return t == BlockType::Nam ? "*.nam" : "*.wav"; }
        juce::File folderFor (BlockType t)    { return t == BlockType::Nam ? paths::namDir() : paths::irDir(); }
    }

    BlockEditor::BlockEditor (RigModel& m) : model (m)
    {
        // 카테고리 목록 순서 = 이 순서. 카테고리 추가는 여기에 한 줄 (파일 없는 카테고리는 내장 블록 목록만 채우면 된다)
        categories = {
            { "Built-in", false, BlockType::None, { { "Empty (pass-through)", BlockType::None } } },
            { "NAM",   true,  BlockType::Nam,  {} },
            { "IR",    true,  BlockType::Ir,   {} },
        };

        // 기본 열 폭 비율 (NAM 2:2:1:1:1). 사용자가 바꾼 폭은 이 표 객체에 남아 프로그램이 꺼질 때까지 유지된다
        const int columnFlags = juce::TableHeaderComponent::visible | juce::TableHeaderComponent::resizable
                              | juce::TableHeaderComponent::sortable;
        const std::tuple<Column, const char*, int> namColumns[] = {
            { nameColumn, "Name", 2 }, { gearModelColumn, "Gear Model", 2 }, { gearTypeColumn, "Gear Type", 1 },
            { toneColumn, "Tone", 1 }, { modeledByColumn, "Modeled By", 1 },
        };

        // 네 표 공통: 머리글(정렬, 열 폭, 열 숨기기 메뉴)도 왼쪽 클릭만 (마우스 규칙), 같은 머리글 모양, 행 높이, 테두리
        for (auto* list : { &categoryList, &namList, &irList, &builtInList })
        {
            list->setHeader (std::make_unique<LeftClickOnly<juce::TableHeaderComponent>>());
            list->getHeader().setStretchToFitActive (true);
            list->getHeader().setLookAndFeel (&headerLookAndFeel);
            list->setRowHeight (24);
            list->setOutlineThickness (1);
        }

        // 카테고리 목록: 둘러보기만 (블록 지정은 오른쪽 목록에서 더블클릭할 때). 열 하나라 폭 조절, 정렬 없음
        categoryList.getHeader().addColumn ("Category", 1, 100, 30, -1, juce::TableHeaderComponent::visible);
        categoryList.setModel (&categoryListModel);
        addAndMakeVisible (categoryList);

        for (const auto& [id, name, ratio] : namColumns)
            namList.getHeader().addColumn (name, id, ratio * 100, 60, -1, columnFlags);

        irList.getHeader().addColumn ("Filename", nameColumn, 100, 60, -1, columnFlags);

        for (auto* list : { &namList, &irList })
        {
            list->setModel (this);
            list->getHeader().setSortColumnId (nameColumn, true);
            addChildComponent (list);
        }

        builtInList.getHeader().addColumn ("Name", 1, 100, 30, -1, juce::TableHeaderComponent::visible);
        builtInList.setModel (&builtInListModel);
        addChildComponent (builtInList);

        // 더블클릭(왼쪽)해야 블록에 지정된다
        fileRowMouse.onDoubleClick = [this] (int row) { assignFile (row); };

        // 행 컨텍스트 메뉴는 파일 표(NAM / IR)만 지원한다. 지금은 임시 항목 (동작 없음)
        fileRowMouse.fillMenu = [] (juce::PopupMenu& menu, int) { menu.addItem ("Temporary item", [] {}); };
        builtInRowMouse.onDoubleClick = [this] (int row)
        {
            const auto& builtIns = shownCategory().builtIns;

            if (model.getSelectedSlot() != nullptr && juce::isPositiveAndBelow (row, (int) builtIns.size()))
                model.assignBlock (model.getSelectedIndex(), builtIns[(size_t) row].type, {});
        };

        model.addChangeListener (this);
        refresh();
    }

    BlockEditor::~BlockEditor()
    {
        model.removeChangeListener (this);
        namList.setModel (nullptr);
        irList.setModel (nullptr);
        builtInList.setModel (nullptr);
        categoryList.setModel (nullptr);
    }

    void BlockEditor::changeListenerCallback (juce::ChangeBroadcaster*)
    {
        updateFromModel();
    }

    const juce::String& BlockEditor::Entry::get (int columnId) const
    {
        switch (columnId)
        {
            case gearModelColumn:   return gearModel;
            case gearTypeColumn:    return gearType;
            case toneColumn:        return tone;
            case modeledByColumn:   return modeledBy;
            default:                return displayName;
        }
    }

    BlockEditor::Entry BlockEditor::makeEntry (const juce::File& file, bool readMetadata)
    {
        NamMetadata metadata;

        if (readMetadata)
        {
            juce::String error;
            metadata = readNamMetadata (file, error).value_or (NamMetadata {});
        }

        return { file,
                 metadata.name.isNotEmpty() ? metadata.name : file.getFileName(),
                 metadata.gearModel,
                 metadata.gearType, metadata.toneType, metadata.modeledBy };
    }

    // 추후 설정창 refresh library 버튼에서 호출 예정
    // nam/, ir/ 폴더를 스캔하고 NAM 메타데이터를 읽어 캐시에 둔다 (디스크를 읽는 곳은 여기뿐)
    void BlockEditor::refresh()
    {
        for (const auto type : { BlockType::Nam, BlockType::Ir })
        {
            const auto folder = folderFor (type);
            auto& entries = type == BlockType::Nam ? namEntries : irEntries;
            entries.clear();

            auto found = folder.findChildFiles (juce::File::findFiles, true, patternFor (type));
            found.sort();

            for (const auto& f : found)
                entries.push_back (makeEntry (f, type == BlockType::Nam));
        }

        outsideEntry = {};
        shownSlotId = 0;   // 목록이 바뀌었으니 강조도 다시 맞춘다
        updateFromModel();
    }

    int BlockEditor::categoryFor (BlockType type) const
    {
        for (size_t i = 0; i < categories.size(); ++i)
        {
            const auto& c = categories[i];

            if (c.usesFiles ? c.fileType == type
                            : std::any_of (c.builtIns.begin(), c.builtIns.end(), [type] (const BuiltIn& d) { return d.type == type; }))
                return (int) i;
        }

        return 0;
    }

    // 모델 변경 알림마다 호출. 둘러보던 탭은 선택 블록이나 그 종류/파일이 바뀌었을 때만 블록의 카테고리로 되돌린다
    void BlockEditor::updateFromModel()
    {
        const auto* slot = model.getSelectedSlot();
        const bool enabled = slot != nullptr;

        for (auto* c : std::initializer_list<juce::Component*> { &namList, &irList, &builtInList })
        {
            c->setEnabled (enabled);
            c->setAlpha (enabled ? 1.0f : 0.4f);
        }

        categoryList.setEnabled (enabled);
        categoryList.setAlpha (enabled ? 1.0f : 0.4f);

        if (slot == nullptr)
        {
            shownSlotId = 0;
            shownCategoryIndex = categoryFor (BlockType::None);
            showCategory (true);
            return;
        }

        const bool reset = slot->id != shownSlotId || slot->type != shownType || slot->file != shownFile;

        if (reset)
        {
            shownSlotId = slot->id;
            shownType = slot->type;
            shownFile = slot->file;
            shownCategoryIndex = categoryFor (slot->type);
        }

        showCategory (reset);
    }

    // shownCategoryIndex 의 탭과 목록을 보여준다 (파일 표는 캐시에서 다시 고른다, 폴더 스캔 없음).
    // resetHighlight 면 강조를 선택 블록의 현재 내장 블록/파일로 (이 카테고리에 없으면 강조 없음)
    void BlockEditor::showCategory (bool resetHighlight)
    {
        const auto* slot = model.getSelectedSlot();
        const auto& category = shownCategory();
        const bool slotInCategory = slot != nullptr && categoryFor (slot->type) == shownCategoryIndex;

        if (categoryList.getSelectedRow() != shownCategoryIndex)
            categoryList.selectRow (shownCategoryIndex);

        namList.setVisible (category.usesFiles && category.fileType == BlockType::Nam);
        irList.setVisible (category.usesFiles && category.fileType == BlockType::Ir);
        builtInList.setVisible (! category.usesFiles);

        if (! category.usesFiles)
        {
            builtInList.updateContent();

            if (resetHighlight)
            {
                builtInList.deselectAllRows();

                for (size_t i = 0; i < category.builtIns.size(); ++i)
                    if (slotInCategory && category.builtIns[i].type == slot->type)
                        builtInList.selectRow ((int) i);
            }

            builtInList.repaint();
            return;
        }

        const bool isNam = category.fileType == BlockType::Nam;

        // 파일 목록: 폴더 스캔 결과 + (목록에 없으면) 현재 파일
        rows = isNam ? namEntries : irEntries;

        const auto findRow = [this] (const juce::File& f)
        {
            for (size_t i = 0; i < rows.size(); ++i)
                if (rows[i].file == f)
                    return (int) i;

            return -1;
        };

        if (slotInCategory && findRow (slot->file) < 0)
        {
            if (outsideEntry.file != slot->file)
                outsideEntry = makeEntry (slot->file, isNam);

            rows.push_back (outsideEntry);
        }

        sortRows();
        auto& list = activeList();
        list.updateContent();

        // 행 수가 바뀌면 세로 스크롤바가 생기거나 사라지는데, JUCE 는 열 폭을 resized() 에서만 맞춘다.
        // 그대로 두면 머리글 오른쪽에 스크롤바 폭만큼 빈 열이 남으므로 다시 맞춘다 (사용자가 바꾼 폭 비율은 유지됨)
        list.resized();

        if (resetHighlight)
        {
            list.deselectAllRows();
            const int current = slotInCategory ? findRow (slot->file) : -1;
            if (current >= 0)
                list.selectRow (current);
        }

        list.repaint();
    }

    int BlockEditor::getNumRows()
    {
        return (int) rows.size();
    }

    void BlockEditor::paintRowBackground (juce::Graphics& g, int row, int width, int height, bool selected)
    {
        drawRowBackground (g, row, width, height, selected, fileRowMouse.isMenuRow (row));
    }

    // 선택 = 강조색, 홀수 행은 살짝 밝게, 아래 구분선. 행 메뉴가 열린 행은 선택을 바꾸지 않고 테두리만 두른다
    void BlockEditor::drawRowBackground (juce::Graphics& g, int row, int width, int height, bool selected, bool menuTarget) const
    {
        if (selected)
            g.fillAll (findColour (juce::TextEditor::highlightColourId));
        else if (row % 2 != 0)
            g.fillAll (juce::Colours::white.withAlpha (0.04f));

        g.setColour (juce::Colours::grey.withAlpha (0.25f));
        g.drawHorizontalLine (height - 1, 0.0f, (float) width);

        if (menuTarget)
        {
            g.setColour (juce::Colours::white);
            g.drawRect (0, 0, width, height, 1);
        }
    }

    int BlockEditor::BuiltInListModel::getNumRows()
    {
        return (int) owner.shownCategory().builtIns.size();
    }

    int BlockEditor::CategoryListModel::getNumRows()
    {
        return (int) owner.categories.size();
    }

    void BlockEditor::CategoryListModel::paintRowBackground (juce::Graphics& g, int row, int width, int height, bool selected)
    {
        owner.drawRowBackground (g, row, width, height, selected, owner.categoryRowMouse.isMenuRow (row));
    }

    void BlockEditor::CategoryListModel::paintCell (juce::Graphics& g, int row, int, int width, int height, bool selected)
    {
        if (juce::isPositiveAndBelow (row, getNumRows()))
            owner.paintRowText (g, owner.categories[(size_t) row].name, width, height, selected);
    }

    void BlockEditor::CategoryListModel::selectedRowsChanged (int lastRowSelected)
    {
        // 행을 비우는 클릭이면 지금 카테고리 강조를 되살린다
        if (lastRowSelected < 0)
        {
            owner.categoryList.selectRow (owner.shownCategoryIndex);
            return;
        }

        if (lastRowSelected != owner.shownCategoryIndex)
        {
            owner.shownCategoryIndex = lastRowSelected;
            owner.showCategory (true);
        }
    }

    void BlockEditor::BuiltInListModel::paintRowBackground (juce::Graphics& g, int row, int width, int height, bool selected)
    {
        owner.drawRowBackground (g, row, width, height, selected, owner.builtInRowMouse.isMenuRow (row));
    }

    void BlockEditor::BuiltInListModel::paintCell (juce::Graphics& g, int row, int, int width, int height, bool selected)
    {
        const auto& builtIns = owner.shownCategory().builtIns;

        if (juce::isPositiveAndBelow (row, (int) builtIns.size()))
            owner.paintRowText (g, builtIns[(size_t) row].name, width, height, selected);
    }

    void BlockEditor::paintCell (juce::Graphics& g, int row, int columnId, int width, int height, bool selected)
    {
        if (! juce::isPositiveAndBelow (row, (int) rows.size()))
            return;

        paintRowText (g, rows[(size_t) row].get (columnId), width, height, selected);

        // 마지막 열의 오른쪽 경계선은 그리지 않는다 (머리글처럼 스크롤바와 이어져 보이게)
        const auto& header = activeList().getHeader();
        if (columnId != header.getColumnIdOfIndex (header.getNumColumns (true) - 1, true))
        {
            g.setColour (juce::Colours::grey.withAlpha (0.25f));
            g.drawVerticalLine (width - 1, 0.0f, (float) height);
        }
    }

    // 칸 글자: 왼쪽 정렬, 좌우 6 px, 넘치면 말줄임 (찌그러뜨리지 않는다)
    void BlockEditor::paintRowText (juce::Graphics& g, const juce::String& text, int width, int height, bool selected) const
    {
        g.setColour (selected ? findColour (juce::TextEditor::highlightedTextColourId)
                              : findColour (juce::ListBox::textColourId));
        g.setFont (juce::FontOptions (rowFontSize));
        g.drawText (text, 6, 0, juce::jmax (0, width - 12), height, juce::Justification::centredLeft, true);
    }

    void BlockEditor::assignFile (int row)
    {
        if (model.getSelectedSlot() == nullptr || ! juce::isPositiveAndBelow (row, (int) rows.size()))
            return;

        const auto file = rows[(size_t) row].file;

        // 목록은 다시 스캔하지 않으므로, 그 사이 지워졌거나 옮겨진 파일일 수 있다
        if (! file.existsAsFile())
        {
            showFileNotFound (file);
            return;
        }

        // 같은 파일이어도 막지 않는다 (로딩 실패 후 다시 시도). 중복 로딩은 RigModel::assignBlock 이 막는다
        model.assignBlock (model.getSelectedIndex(), shownCategory().fileType, file);
    }

    void BlockEditor::sortOrderChanged (int, bool)
    {
        // 정렬 상태는 보이는 표의 머리글에 있다. 정렬 후에도 같은 파일을 강조
        auto& list = activeList();
        const int selected = list.getSelectedRow();
        const auto selectedFile = juce::isPositiveAndBelow (selected, (int) rows.size()) ? rows[(size_t) selected].file
                                                                                          : juce::File();
        sortRows();
        list.updateContent();

        for (size_t i = 0; i < rows.size(); ++i)
            if (selectedFile != juce::File() && rows[i].file == selectedFile)
                list.selectRow ((int) i);

        list.repaint();
    }

    // 캐시된 목록만 정렬한다 (디스크 읽기 없음).
    // 대소문자 무시 자연 정렬, 빈 값은 방향과 관계없이 맨 아래, 같으면 파일명 순
    void BlockEditor::sortRows()
    {
        const auto& header = activeList().getHeader();
        const int column = header.getSortColumnId();
        const bool forwards = header.isSortedForwards();

        std::stable_sort (rows.begin(), rows.end(), [column, forwards] (const Entry& a, const Entry& b)
        {
            const auto& keyA = a.get (column);
            const auto& keyB = b.get (column);

            if (keyA.isEmpty() != keyB.isEmpty())
                return keyB.isEmpty();

            if (const int c = keyA.compareNatural (keyB); c != 0)
                return forwards ? c < 0 : c > 0;

            if (const int c = a.file.getFileName().compareNatural (b.file.getFileName()); c != 0)
                return c < 0;

            return a.file.getFullPathName() < b.file.getFullPathName();
        });
    }

    void BlockEditor::showFileNotFound (const juce::File& file)
    {
        // 설정창처럼 버튼으로만 닫히게: ESC 로 닫히지 않고, 닫기(X) 버튼도 없다
        showAlert (*this, "File not found",
                   file.getFullPathName() + "\n\nThe file may have been moved or deleted. "
                                            "It will disappear from the list after the library is refreshed.",
                   { { "OK", 1 } }, false);
    }

    // LookAndFeel_V3::drawTableHeaderBackground 와 같고, 마지막 열의 오른쪽 경계선만 뺐다
    void BlockEditor::HeaderLookAndFeel::drawTableHeaderBackground (juce::Graphics& g, juce::TableHeaderComponent& header)
    {
        auto r = header.getLocalBounds();
        const auto outlineColour = header.findColour (juce::TableHeaderComponent::outlineColourId);

        g.setColour (outlineColour);
        g.fillRect (r.removeFromBottom (1));

        g.setColour (header.findColour (juce::TableHeaderComponent::backgroundColourId));
        g.fillRect (r);

        g.setColour (outlineColour);

        for (int i = header.getNumColumns (true) - 1; --i >= 0;)
            g.fillRect (header.getColumnPosition (i).removeFromRight (1));
    }

    void BlockEditor::paint (juce::Graphics& g)
    {
        g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.drawHorizontalLine (0, 0.0f, (float) getWidth());
    }

    void BlockEditor::resized()
    {
        // 눈에 보이는 간격을 위쪽 빈 공간(구분선 1 px 아래 11 px)에 맞춘다:
        //  - 위: 12 = 구분선 1 + 빈 공간 11
        //  - 왼쪽/오른쪽 끝: 10 + 창 테두리 1 px(배경과 같은 색) = 11
        //  - 탭-목록: 11
        constexpr int topGap = 12, edgeGap = 10, middleGap = 11, bottomGap = 12;
        auto area = getLocalBounds().withTrimmedTop (topGap).withTrimmedBottom (bottomGap).reduced (edgeGap, 0);

        // 카테고리 목록 열은 고정 폭, 높이는 남은 영역 전체 (넘치면 스크롤)
        categoryList.setBounds (area.removeFromLeft (categoryListWidth));
        area.removeFromLeft (middleGap);

        namList.setBounds (area);
        irList.setBounds (area);
        builtInList.setBounds (area);
    }
}
