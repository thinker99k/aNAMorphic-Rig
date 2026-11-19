#pragma once

#include "model/RigModel.h"
#include "ui/Widgets.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace rig
{
    // Live 화면의 하단 줄: [Live ON] [◀ PREV PATCH] [NEXT PATCH ▶]
    //  - 바깥 여백과 버튼 사이 6 px (편집 화면 SceneBar 와 같은 값). Live ON 은 SceneBar 의 Live 버튼과 같은 사각형
    //  - PREV / NEXT 는 남은 폭을 반반 (NEXT 가 나머지). 이웃 패치의 번호와 이름을 보여준다
    //  - 이웃 패치가 없으면 비활성 ("—", 눌러도 무시). 순환하지 않는다
    //  - 왼쪽 클릭만 받는다. 호버·눌림 효과 없음
    class PatchBar final : public juce::Component,
                           private juce::ChangeListener
    {
    public:
        explicit PatchBar (RigModel& model);
        ~PatchBar() override;

        std::function<void()> onLiveOff;          // Live ON 버튼
        std::function<void (int)> onPatchStep;    // PREV = -1, NEXT = +1

        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent&) override;

    private:
        void changeListenerCallback (juce::ChangeBroadcaster*) override;
        void refreshNeighbors();   // 이웃 패치 이름은 파일에서 읽으므로 패치가 바뀔 때만 부른다

        RigModel& model;
        juce::Rectangle<int> liveBounds, prevBounds, nextBounds;
        int shownPatch = 0;
        bool hasPrev = false, hasNext = false;
        juce::String prevText, nextText;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PatchBar)
    };
}
