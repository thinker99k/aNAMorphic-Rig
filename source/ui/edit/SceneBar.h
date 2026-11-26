#pragma once

#include "model/RigModel.h"
#include "ui/SceneStompSwitch.h"
#include "ui/Widgets.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>

namespace rig
{
    // 화면 아래 ssbar (Scene 모드 = 씬 바, Stomp 모드 = Stomp 바. 같은 자리 8칸이 모드를 따라 바뀐다)
    //   [Scene / Stomp Reset] [ ] [Rename / Reset | Bind / Unbind] [1] ... [8] [Copy / Swap] [Scene|Stomp] [Live]
    //  - 버튼은 모두 14칸 격자의 한 칸 (bottomBarCell, Live statbar 와 같음). 바깥 여백과 버튼 사이 모두 6 px.
    //    칸 버튼: 번호는 왼쪽 위, 이름은 아래 가운데
    //  - 0번 칸 = 큰 리셋 (Scene Reset... / Stomp Reset...), 2번 칸 = 모드별 두 버튼 위아래, 8칸은 3 ~ 10번,
    //    11번 칸 = Copy / Swap 위아래 (두 모드 공용), 12번 칸 = Scene|Stomp 스위치 (SceneStompSwitch), 13번 칸 = Live
    //  - 컨텍스트 메뉴 없음 (모바일 대비). 버튼은 왼쪽 클릭에만 반응한다 (Widgets.h LeftClickOnly)
    //  - Scene 모드
    //      칸 클릭 = 그 씬으로 바로 전환 (더블클릭 기능 없음)
    //      Rename : 현재 씬 버튼 자리에 입력칸 (글자는 이름처럼 아래 가운데, 오른쪽 위 귀퉁이에 정사각형 체크 버튼).
    //               체크 버튼이나 Enter 로 저장, 바깥 클릭·Esc 는 취소 (16자)
    //      Reset  : 현재 씬의 모든 블록 On, 이름은 "Scene n" (중간 리셋이라 경고 없이 바로)
    //      Scene Reset... : 씬 8개를 한꺼번에 초기화 (경고창에서 Reset 을 눌러야)
    //  - Stomp 모드
    //      칸 클릭 = 묶인 블록 켜고 끄기 (RigModel::toggleStomp, 저장 안 함). 안 묶인 칸은 동작 없음
    //      Bind   : 고르는 모드. 칸을 누르면 패치맵에서 선택한 블록을 그 칸에 묶고 끝 (블록을 선택해야 활성)
    //      Unbind : 고르는 모드. 칸을 누르면 그 칸을 풀고 끝 (묶인 칸이 있어야 활성)
    //      Stomp Reset... : 8칸 모두 풀기 (경고창에서 Reset 을 눌러야)
    //  - 고르는 모드 (Copy / Swap / Bind / Unbind): 같은 버튼을 다시 누르면 끝, 다른 버튼을 누르면 그 모드로 처음부터.
    //      Copy / Swap 은 칸을 src → dst 순서로 고르면 바로 실행하고 끝 (src 를 다시 누르면 무시).
    //      ESC 나 ssbar 바깥 클릭은 취소 (바깥 클릭의 원래 동작은 그대로 일어난다). 현재 씬 번호는 바뀌지 않는다
    //  - 표시: 평소에는 현재 씬 / 켜진 Stomp 를 밝게 채운다. 모드 중에는 강조를 끄고 모드 버튼을 켜며,
    //      고른 src 는 밝기가 오르내린다. 실행 뒤 테두리가 부드럽게 두 번 깜빡인다 (Copy·Bind = dst, Swap = 둘 다)
    //  - 마우스를 올려도 칸과 Live 버튼은 강조하지 않는다 (Live 와 연관된 요소)
    class SceneBar final : public juce::Component,
                           private juce::ChangeListener,
                           private juce::Timer
    {
    public:
        explicit SceneBar (RigModel& model);
        ~SceneBar() override;

        std::function<void()> onLiveClicked;   // Live 버튼

        void cancelEditing();   // 진행 중인 고르는 모드와 씬 이름 편집을 끝낸다

        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent&) override;
        bool keyPressed (const juce::KeyPress&) override;

    private:
        enum class Mode { none, copy, swap, bind, unbind };

        // 고르는 모드 중 ssbar 바깥 클릭을 알아차린다 (클릭 자체는 막지 않는다)
        struct OutsideClickWatcher final : public juce::MouseListener
        {
            explicit OutsideClickWatcher (SceneBar& b) : owner (b) {}
            void mouseDown (const juce::MouseEvent&) override;
            SceneBar& owner;
        };

        void changeListenerCallback (juce::ChangeBroadcaster*) override;
        void timerCallback() override;

        bool stomp() const { return model.isStompMode(); }
        int cellAt (juce::Point<int> position) const;
        void cellClicked (int cell);
        void toggleMode (Mode newMode);
        void cancelMode();
        void updateButtons();   // 모드별 글자·보이기·활성
        void startRename (int scene);
        void cancelRename();
        void layoutNameEditor();
        void flash (int cell);
        void updateTimer();

        RigModel& model;

        std::array<juce::Rectangle<int>, RigModel::numScenes> cellBounds;
        static_assert (RigModel::numScenes == RigModel::numStomps);
        LeftClickOnly<juce::TextButton> liveButton { "Live" };
        SceneStompSwitch modeSwitch;
        LeftClickOnly<juce::TextButton> copyButton { "Copy" }, swapButton { "Swap" };
        LeftClickOnly<juce::TextButton> renameButton { "Rename" }, sceneResetButton { "Reset" };   // Scene 모드 2번 칸
        LeftClickOnly<juce::TextButton> bindButton { "Bind" }, unbindButton { "Unbind" };          // Stomp 모드 2번 칸
        LeftClickOnly<juce::TextButton> resetAllButton { "Scene\nReset..." };   // 큰 리셋: 말줄임표 (경고창이 뜬다). Stomp 모드면 "Stomp\nReset..."

        Mode mode = Mode::none;
        int sourceCell = -1;                    // Copy / Swap 중 고른 src (-1 = 아직)
        std::array<juce::uint32, RigModel::numScenes> flashStart {};   // 실행 뒤 테두리 강조 시작 시각 (0 = 없음)
        OutsideClickWatcher outsideClickWatcher { *this };

        PassThroughLabel nameEditor;                       // 이름 편집 (공용, Widgets.h)
        CheckCornerButton saveNameButton { "Save scene name", 5.0f, 0.5f };   // 씬 버튼 오른쪽 위 귀퉁이 (반지름 5 = sceneCorner)
        int renamingScene = -1;                 // 입력칸이 덮고 있는 씬 (-1 = 편집 중 아님)
        int nameTargetScene = -1;               // 입력칸 내용을 저장할 씬. 편집기가 닫힌 뒤에 저장 알림이 오므로 따로 둔다

        int shownPatch = 0;                     // 패치가 바뀌면 모드와 이름 편집을 취소한다
        bool shownStomp = false;                // Scene / Stomp 가 바뀌어도 취소한다
        HighlightRepaint highlightRepaint { *this, [this] { return model.getHighlightStart(); } };   // Undo / Redo 강조가 사라지는 동안

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SceneBar)
    };
}
