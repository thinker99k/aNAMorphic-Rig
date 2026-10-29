#pragma once

#include "model/RigModel.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace rig
{
    // 화면 상단 절반: 직렬 체인.
    //   IN -> [블록1] -> [블록2] -> ... -> [+ Add] -> OUT
    // 블록 수 제한 없음. 넘치면 가로 스크롤.
    class ChainView final : public juce::Component,
                            private juce::ChangeListener
    {
    public:
        explicit ChainView (RigModel& model);
        ~ChainView() override;

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        class Tile;
        class Strip;

        void changeListenerCallback (juce::ChangeBroadcaster*) override;
        void rebuild();
        void showAddMenu();

        RigModel& model;
        juce::Viewport viewport;
        std::unique_ptr<Strip> strip;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChainView)
    };
}
