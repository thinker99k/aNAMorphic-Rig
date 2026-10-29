#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace rig
{
    // 상단 바: [Patch 콤보박스] ............................ [Settings 버튼]
    class TopBar final : public juce::Component
    {
    public:
        TopBar (int numPatches);

        void setPatchNumber (int number);

        std::function<void (int)> onPatchChanged;
        std::function<void()> onSettingsClicked;

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        juce::Label patchLabel { {}, "Patch" };
        juce::ComboBox patchBox;
        juce::TextButton settingsButton { "Settings..." };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopBar)
    };
}
