#pragma once

#include "model/RigModel.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace rig
{
    // 화면 상단: 패치 맵 (직렬 체인).
    //   [IN] -+- [블록1] -+- [블록2] -+- [OUT]
    // 이웃한 두 노드 사이 연결선 가운데의 [+] 를 누르면 그 자리에 미지정 블록이 들어간다.
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

        RigModel& model;
        juce::Viewport viewport;
        std::unique_ptr<Strip> strip;
        juce::uint64 shownSelectedId = 0;   // 선택이 바뀌었을 때만 그 블록이 보이도록 스크롤

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChainView)
    };
}
