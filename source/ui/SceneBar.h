#pragma once

#include "model/RigModel.h"
#include "ui/Widgets.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>

namespace rig
{
    // 화면 아래 씬 바: [Live]      [1] [2] ... [8]      [Copy / Swap][All Reset]
    //  - 씬 버튼과 Live 는 정사각형. 오른쪽 묶음은 씬 버튼의 1.75 배 폭 (씬 묶음과 겹치지 않는 만큼까지).
    //    바깥 여백과 묶음 안 간격은 6 px. 씬 버튼: 번호는 왼쪽 위, 이름은 아래 가운데
    //  - Live 는 왼쪽 끝, 씬 버튼 8개는 가로 가운데, [Copy/Swap | All Reset] 는 오른쪽 끝
    //  - Live 는 아직 UI 만 (비활성)
    //  - Copy / Swap / All Reset 은 왼쪽 클릭에만 반응한다
    //  - 왼쪽 버튼을 누르는 순간 그 씬으로 전환 (더블클릭 기능 없음)
    //  - 오른쪽 클릭: 전환하지 않고 Rename / Reset 메뉴만 (머리글 없음)
    //      Rename : 그 버튼 자리에 입력칸 (글자는 이름처럼 아래 가운데, 오른쪽 위 귀퉁이에 딱 붙은 정사각형 체크 버튼).
    //               체크 버튼이나 Enter 로 저장, 바깥 클릭은 취소 (16자)
    //      Reset  : 그 씬의 모든 블록 On, 이름은 "Scene n" (경고 없이 바로)
    //  - Copy / Swap 모드: 같은 버튼을 다시 누르면 끝, 다른 버튼을 누르면 그 모드로 처음(src)부터.
    //      씬을 src → dst 순서로 고르면 바로 실행하고 모드를 끝낸다 (src 를 다시 누르면 무시).
    //      ESC 나 씬 바 바깥 클릭은 취소 (바깥 클릭의 원래 동작은 그대로 일어난다).
    //      현재 씬 번호는 바뀌지 않는다
    //  - 표시: 평소에는 현재 씬을 밝게 채운다. 모드 중에는 1~8 강조를 끄고 Copy / Swap 버튼을 켜며,
    //      고른 src 는 밝기가 오르내린다. 실행 뒤 테두리가 부드럽게 두 번 깜빡인다 (Copy = dst, Swap = 둘 다)
    //  - All Reset: 씬 8개를 한꺼번에 초기화하므로 경고창에서 Reset 을 눌러야 초기화 (모든 블록 On, 기본 이름).
    //      모드 중이면 모드를 끝낸다. 오른쪽 클릭 메뉴의 씬 하나 Reset 은 경고 없이 바로
    class SceneBar final : public juce::Component,
                           private juce::ChangeListener,
                           private juce::Timer
    {
    public:
        explicit SceneBar (RigModel& model);
        ~SceneBar() override;

        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent&) override;
        bool keyPressed (const juce::KeyPress&) override;

    private:
        enum class Mode { none, copy, swap };

        // Label 은 편집 중에 모달이 되어, 바깥 클릭은 컴포넌트에 닿기 전에 편집을 끝내 버린다 (TopBar 와 같은 방식).
        // 체크 버튼 클릭만은 그대로 통과시켜야 저장할 수 있다
        struct SceneNameLabel final : public juce::Label
        {
            std::function<bool (const juce::Component*)> allowWhileEditing;

            bool canModalEventBeSentToComponent (const juce::Component* target) override
            {
                return allowWhileEditing != nullptr && allowWhileEditing (target);
            }
        };

        // Copy / Swap 모드 중 씬 바 바깥 클릭을 알아차린다 (클릭 자체는 막지 않는다)
        struct OutsideClickWatcher final : public juce::MouseListener
        {
            explicit OutsideClickWatcher (SceneBar& b) : owner (b) {}
            void mouseDown (const juce::MouseEvent&) override;
            SceneBar& owner;
        };

        void changeListenerCallback (juce::ChangeBroadcaster*) override;
        void timerCallback() override;

        int sceneAt (juce::Point<int> position) const;
        void sceneClicked (int scene);
        void showSceneMenu (int scene);
        void toggleMode (Mode newMode);
        void cancelMode();
        void startRename (int scene);
        void cancelRename();
        void layoutNameEditor();
        void flash (int scene);
        void updateTimer();

        RigModel& model;

        std::array<juce::Rectangle<int>, RigModel::numScenes> sceneBounds;
        juce::TextButton liveButton { "Live" };
        LeftClickTextButton copyButton { "Copy" }, swapButton { "Swap" }, resetAllButton { "All Reset" };

        Mode mode = Mode::none;
        int sourceScene = -1;                   // 모드 중 고른 src (-1 = 아직)
        std::array<juce::uint32, RigModel::numScenes> flashStart {};   // 실행 뒤 테두리 강조 시작 시각 (0 = 없음)
        OutsideClickWatcher outsideClickWatcher { *this };

        SceneNameLabel nameEditor;
        // 이름 입력칸의 저장 버튼: 씬 버튼 오른쪽 위 귀퉁이에 딱 붙은 정사각형 (블록 삭제 X 와 같은 공용 CornerButton)
        struct SaveNameButton final : public CornerButton
        {
            SaveNameButton();
            void drawContent (juce::Graphics&, juce::Rectangle<float> area, bool active) override;
        };

        SaveNameButton saveNameButton;
        int renamingScene = -1;                 // 입력칸이 덮고 있는 씬 (-1 = 편집 중 아님)
        int nameTargetScene = -1;               // 입력칸 내용을 저장할 씬. 편집기가 닫힌 뒤에 저장 알림이 오므로 따로 둔다

        int shownPatch = 0;                     // 패치가 바뀌면 모드와 이름 편집을 취소한다

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SceneBar)
    };
}
