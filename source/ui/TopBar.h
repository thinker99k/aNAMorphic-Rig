#pragma once

#include "ui/Widgets.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace rig
{
    // 상단 바: [Patch 콤보박스] [이전] [다음]     [패치 이름]     [Settings 버튼]
    //  - 패치 이름 오른쪽 클릭: Rename / Reset 메뉴 (씬 버튼과 같은 방식, 머리글 없음). 왼쪽 클릭과 더블클릭은 동작 없음
    //      Rename : 그 자리에서 편집 (글자 위치는 평소 표시와 같다). 입력칸 오른쪽 끝에 정사각형 체크 버튼.
    //               체크 버튼이나 Enter 로 저장, 바깥 클릭이나 Esc 는 취소. 패치를 바꾸면 편집 취소
    //      Reset  : 경고창에서 Reset 을 누르면 onPatchReset (지금 패치를 기본 패치로. 블록 전체 + 씬 8개 = 큰 리셋)
    class TopBar final : public juce::Component
    {
    public:
        TopBar (int numPatches);

        void setPatchNumber (int number);
        void setPatchName (const juce::String& name);

        std::function<void (int)> onPatchChanged;
        std::function<void (const juce::String&)> onPatchNameChanged;
        std::function<void()> onPatchReset;
        std::function<void()> onSettingsClicked;

        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent&) override;   // 패치 이름의 오른쪽 클릭 (patchNameBox 의 마우스 이벤트를 받는다)

    private:
        void selectAdjacentPatch (int delta);
        void showPatchNameMenu (const juce::MouseEvent& click);
        void resetPatchWithWarning();
        void updateNavigationButtons();

        juce::Label patchLabel { {}, "Patch" };
        LeftClickOnly<juce::ComboBox> patchBox;
        LeftClickOnly<juce::DrawableButton> previousPatchButton { "Previous patch", juce::DrawableButton::ImageOnButtonBackground };
        LeftClickOnly<juce::DrawableButton> nextPatchButton { "Next patch", juce::DrawableButton::ImageOnButtonBackground };
        PassThroughLabel patchNameBox;                                        // 이름 편집 (공용, Widgets.h)
        CheckCornerButton saveNameButton { "Save patch name", 0.0f, 0.0f };   // 입력칸 오른쪽 끝, 편집 중에만 보인다
        LeftClickOnly<juce::TextButton> settingsButton { "Settings..." };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopBar)
    };
}
