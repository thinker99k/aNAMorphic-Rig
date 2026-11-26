#pragma once

#include "model/RigModel.h"
#include "ui/SceneStompSwitch.h"
#include "ui/Widgets.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>
#include <optional>

namespace rig
{
    // 화면 아래 ssbar (Scene 모드 = 씬 바, Stomp 모드 = Stomp 바. 같은 자리 8칸이 모드를 따라 바뀐다)
    //   Scene: [Scene Reset...] [ ] [Rename / Reset] [1] ... [8] [ ] [Scene|Stomp] [Live]
    //   Stomp: [Stomp Reset...] [ ] [ ]              [1] ... [8] [ ] [Scene|Stomp] [Live]
    //  - 버튼은 모두 14칸 격자의 한 칸 (bottomBarCell, Live statbar 와 같음). 바깥 여백과 버튼 사이 모두 6 px
    //  - 컨텍스트 메뉴 없음 (모바일 대비). 왼쪽 클릭만 (Widgets.h LeftClickOnly / isLeftClick)
    //  - 칸 클릭은 끌지 않고 뗐을 때 (끌기와 구분, Live 패드는 누르는 순간): Scene = 그 씬으로 전환, Stomp = 묶인 블록 켜고 끄기
    //  - 칸 모양: Scene = 번호 왼쪽 위 + 이름 가운데 (자르지 않고 글꼴만 줄인다), 현재 씬은 밝게.
    //             Stomp = 묶인 블록 모양 (block::drawStomp, 번호·중요 값 없음, 꺼지면 흐리게), 안 묶인 칸은 빈 칸
    //  - 끌어 놓기 (드래그앤드롭 원칙: 추가 / 삭제 / 스왑 / 이동 / 대체 / 복제). 자기 칸은 놓을 자리가 아니다
    //      Scene 칸 → 다른 칸   : 위 절반 = 복제 (+), 아래 절반 = 스왑 (⇄). 바깥에 놓으면 아무 일 없음
    //      Stomp 칸 → 묶인 칸   : 위 절반 = 복제 (+), 아래 절반 = 스왑 (⇄)
    //      Stomp 칸 → 빈 칸     : 위 절반 = 추가 (+), 아래 절반 = 이동 (표시 없음)
    //      Stomp 칸 → 둘레 밖   : 삭제 (X). 둘레 = 원래 칸 + 사방 keepMargin (다른 칸 위면 그 칸이 우선)
    //      패치맵 블록 → Stomp 칸 (Stomp 모드): 빈 칸 = 추가 (+), 묶인 칸 = 대체 (↔). 칸을 나누지 않는다
    //      빈 Stomp 칸은 끌 수 없다
    //    끄는 동안 놓을 수 있는 칸을 희미하게, 나뉜 칸은 가운데 선 + 반쪽마다 표시 기호, 커서 밑 자리는 진하게.
    //    놓은 뒤 바뀐 칸 테두리가 부드럽게 두 번 깜빡인다 (스왑·이동 = 두 칸)
    //  - Rename : 현재 씬 버튼 자리에 입력칸 (글자는 이름처럼 가운데, 오른쪽 위 귀퉁이에 정사각형 체크 버튼).
    //             체크 버튼이나 Enter 로 저장, 바깥 클릭·Esc 는 취소 (16자)
    //    Reset  : 현재 씬의 모든 블록 On, 이름은 "Scene n" (중간 리셋이라 경고 없이 바로)
    //    Scene Reset... / Stomp Reset... : 씬 8개 초기화 / 8칸 모두 풀기 (경고창에서 Reset 을 눌러야)
    //  - 마우스를 올려도 칸과 Live 버튼은 강조하지 않는다 (Live 와 연관된 요소)
    class SceneBar final : public juce::Component,
                           public juce::DragAndDropTarget,
                           private juce::ChangeListener,
                           private juce::Timer
    {
    public:
        explicit SceneBar (RigModel& model);
        ~SceneBar() override;

        std::function<void()> onLiveClicked;   // Live 버튼

        void cancelEditing();   // 씬 이름 편집을 끝낸다

        // EditView (끌어 놓기 컨테이너) 가 알려 준다: 끌기 시작 / 끝. 끄는 동안 놓을 수 있는 칸을 보여 준다
        void dragStarted (const juce::var& description);
        void dragEnded();

        // 패치맵 블록을 끄는 중: 화면 좌표 screen 에서 떼면 일어날 일 (Stomp 칸 위 = 추가 / 대체, 그 밖 = none)
        block::DragMark mapDropMark (juce::Point<int> screen) const;
        juce::Image stompDragImage (juce::uint64 slotId, block::DragMark) const;   // Stomp 칸 모양 끄는 그림 (칸 크기)

        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;

        bool isInterestedInDragSource (const SourceDetails&) override;
        void itemDragMove (const SourceDetails&) override;
        void itemDragExit (const SourceDetails&) override;
        void itemDropped (const SourceDetails&) override;

    private:
        // 끄는 것: ssbar 칸 하나 (cell) 또는 패치맵 블록 하나 (slotId)
        struct DragSource
        {
            int cell = -1;
            juce::uint64 slotId = 0;
        };

        // 놓을 자리 하나와 그때 할 일
        enum class Action { copy, swap, move, add, replace };   // add = 빈 칸에 (칸 복제 또는 블록 묶기), replace = 묶인 칸을 새 블록으로

        struct Drop
        {
            int cell = -1;
            bool upper = false;    // 칸을 위아래로 나눌 때 위 절반인지
            bool split = false;    // 칸을 나누는지 (ssbar 칸을 끌 때)
            Action action = Action::copy;
        };

        void changeListenerCallback (juce::ChangeBroadcaster*) override;
        void timerCallback() override;

        bool stomp() const { return model.isStompMode(); }
        bool bound (int cell) const { return model.getStompSlot (cell) != nullptr; }
        int cellAt (juce::Point<int> position) const;
        std::optional<DragSource> readSource (const juce::var& description) const;
        std::optional<Drop> dropAt (const DragSource&, juce::Point<int> position) const;   // 그 자리에 놓을 수 없으면 nullopt
        static block::DragMark markFor (const Drop&);
        void perform (const DragSource&, const Drop&);
        bool isDeleteAt (juce::Point<int> screen) const;   // 끌던 Stomp 칸을 여기서 떼면 삭제인지
        void cellClicked (int cell);
        void updateButtons();   // 모드별 글자·보이기
        void startRename (int scene);
        void cancelRename();
        void layoutNameEditor();
        void flash (int cell);
        void updateTimer();
        void paintDropHints (juce::Graphics&);

        RigModel& model;

        std::array<juce::Rectangle<int>, RigModel::numScenes> cellBounds;
        static_assert (RigModel::numScenes == RigModel::numStomps);
        LeftClickOnly<juce::TextButton> liveButton { "Live" };
        SceneStompSwitch modeSwitch;
        LeftClickOnly<juce::TextButton> renameButton { "Rename" }, sceneResetButton { "Reset" };   // Scene 모드 [3]
        LeftClickOnly<juce::TextButton> resetAllButton { "Scene\nReset..." };   // 큰 리셋: 말줄임표 (경고창이 뜬다). Stomp 모드면 "Stomp\nReset..."

        int pressedCell = -1;                    // 왼쪽 버튼을 누른 칸 (뗄 때 같은 칸이면 클릭)
        std::optional<DragSource> dragSource;    // 끄는 중인 것 (ssbar 칸이든 패치맵 블록이든)
        std::optional<Drop> hoverDrop;           // 커서 밑 놓을 자리
        juce::Image dragSnapshot;                // ssbar 칸을 끌기 시작할 때 칸 그림 (표시를 바꿀 때 다시 쓴다)
        block::DragMark shownMark = block::DragMark::none;
        std::array<juce::uint32, RigModel::numScenes> flashStart {};   // 놓은 뒤 테두리 강조 시작 시각 (0 = 없음)

        PassThroughLabel nameEditor;                       // 이름 편집 (공용, Widgets.h)
        CheckCornerButton saveNameButton { "Save scene name", 5.0f, 0.5f };   // 씬 버튼 오른쪽 위 귀퉁이 (반지름 5 = sceneCorner)
        int renamingScene = -1;                 // 입력칸이 덮고 있는 씬 (-1 = 편집 중 아님)
        int nameTargetScene = -1;               // 입력칸 내용을 저장할 씬. 편집기가 닫힌 뒤에 저장 알림이 오므로 따로 둔다

        int shownPatch = 0;                     // 패치가 바뀌면 이름 편집을 취소한다
        bool shownStomp = false;                // Scene / Stomp 가 바뀌어도 취소한다
        HighlightRepaint highlightRepaint { *this, [this] { return model.getHighlightStart(); } };   // Undo / Redo 강조가 사라지는 동안

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SceneBar)
    };
}
