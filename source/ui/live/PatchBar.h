#pragma once

#include "model/RigModel.h"
#include "ui/Widgets.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace rig
{
    // Live 화면의 위 줄 (편집 화면 TopBar 자리): [◀ 이전 번호 1/8] [지금 번호 1/8 | 패치 이름 5/8] [다음 번호 ▶ 1/8]
    //  - 바깥 여백과 칸 사이 6 px (편집 화면 SceneBar 와 같은 값)
    //  - 이전 / 다음은 화살표와 이웃 패치 번호 (두 자리, 씬 패드 번호와 같은 글꼴)만. 이웃이 없으면 비활성 (번호 없음, 눌러도 무시).
    //    순환하지 않는다
    //  - 지금 패치 번호와 이름은 어두운 표시 칸 하나에 세로 구분선(위아래 끝까지 잇지 않음)으로 나눠 굵게, 표시만 (이름 바꾸기·리셋은 편집 화면 TopBar 에서). 넘치면 "..."
    //  - 왼쪽 클릭만 받는다. 호버·눌림 효과 없음
    class PatchBar final : public juce::Component,
                           private juce::ChangeListener
    {
    public:
        explicit PatchBar (RigModel& model);
        ~PatchBar() override;

        std::function<void (int)> onPatchStep;    // 이전 = -1, 다음 = +1

        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent&) override;

    private:
        void changeListenerCallback (juce::ChangeBroadcaster*) override;
        bool hasPrev() const { return model.getPatchNumber() > 1; }
        bool hasNext() const { return model.getPatchNumber() < RigModel::numPatches; }

        RigModel& model;
        juce::Rectangle<int> prevBounds, nameBounds, nextBounds;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PatchBar)
    };
}
