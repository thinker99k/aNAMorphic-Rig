#pragma once

#include "model/RigModel.h"
#include "ui/live/PatchBar.h"
#include "ui/live/ScenePads.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace rig
{
    // Live 화면 (TopBar 와 StatusBar 사이). 편집 기능 없이 씬 패드를 크게 눌러 전환한다
    //  ┌──────────────────────────────────────────────┐
    //  │ ScenePads : 씬 패드 4 x 2 (나머지)            │
    //  ├──────────────────────────────────────────────┤
    //  │ PatchBar  : [Live ON] [◀ PREV] [NEXT ▶] (1/8) │  편집 화면의 씬 바와 같은 자리
    //  └──────────────────────────────────────────────┘
    class LiveView final : public juce::Component
    {
    public:
        explicit LiveView (RigModel& model);

        std::function<void()> onLiveOff;          // Live ON 버튼
        std::function<void (int)> onPatchStep;    // PREV = -1, NEXT = +1

        void resized() override;

    private:
        ScenePads scenePads;
        PatchBar patchBar;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LiveView)
    };
}
