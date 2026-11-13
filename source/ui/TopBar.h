#pragma once

#include "ui/Widgets.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace rig
{
    // 상단 바: [Patch 콤보박스] [이전] [다음]     [패치 이름][연필][Reset]     [Settings 버튼]
    //  - Reset: 경고창에서 Reset 을 누르면 onPatchReset (지금 패치를 기본 패치로. 블록 전체 + 씬 8개 = 큰 리셋)
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

    private:
        // Label 은 편집 중에 모달이 되어, 바깥 클릭은 컴포넌트에 닿기 전에 편집을 끝내 버린다.
        // 체크 버튼 클릭만은 그대로 통과시켜야 저장할 수 있다.
        struct PatchNameLabel final : public juce::Label
        {
            std::function<bool (const juce::Component*)> allowWhileEditing;

            bool canModalEventBeSentToComponent (const juce::Component* target) override
            {
                return allowWhileEditing != nullptr && allowWhileEditing (target);
            }
        };

        void selectAdjacentPatch (int delta);
        void updateNavigationButtons();

        juce::Label patchLabel { {}, "Patch" };
        juce::ComboBox patchBox;
        juce::DrawableButton previousPatchButton { "Previous patch", juce::DrawableButton::ImageOnButtonBackground };
        juce::DrawableButton nextPatchButton { "Next patch", juce::DrawableButton::ImageOnButtonBackground };
        PatchNameLabel patchNameBox;
        juce::DrawableButton editNameButton { "Edit patch name",
                                             juce::DrawableButton::ImageOnButtonBackground };
        juce::DrawablePath pencilImage, checkImage;   // 편집 중에는 연필 → 체크 (체크를 눌러야 저장)
        LeftClickTextButton resetPatchButton { "Reset" };
        juce::TextButton settingsButton { "Settings..." };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopBar)
    };
}
