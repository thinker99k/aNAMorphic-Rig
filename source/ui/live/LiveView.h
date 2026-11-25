#pragma once

#include "engine/AudioEngine.h"
#include "model/RigModel.h"
#include "ui/live/LiveStatusBar.h"
#include "ui/live/PatchBar.h"
#include "ui/live/ScenePads.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace rig
{
    // Live 화면 (창 맨 위부터 StatusBar 위까지, 편집 화면의 TopBar + EditView 자리). 편집 기능 없이 씬 패드를 크게 눌러 전환한다
    //  ┌──────────────────────────────────────────────┐
    //  │ PatchBar      : [◀ 이전] 패치 이름 [다음 ▶]   │  barHeight
    //  ├──────────────────────────────────────────────┤
    //  │ ScenePads     : 씬 패드 4 x 2 (나머지)        │
    //  ├──────────────────────────────────────────────┤
    //  │ LiveStatusBar : 채널 / 튜너 / 모드 / Live      │  barHeight (편집 화면 씬 바와 같은 높이·자리)
    //  └──────────────────────────────────────────────┘
    class LiveView final : public juce::Component
    {
    public:
        LiveView (RigModel& model, AudioEngine& engine, juce::AudioDeviceManager& deviceManager);

        std::function<void()> onLiveOff;          // Live 버튼
        std::function<void (int)> onPatchStep;    // 이전 = -1, 다음 = +1

        // 위·아래 줄 높이. 편집 화면 씬 바와 같게 MainComponent 가 넣는다 (모드를 바꿔도 Live 버튼이 같은 자리)
        void setBarHeight (int height);

        // Live 에 들어갈 때: 채널은 지금 패치의 채널로 시작, 튜너는 꺼진 상태
        // 나갈 때: 튜너를 끄고 (무음 해제) 패치의 채널로 돌아간다
        void enter();
        void leave();

        void resized() override;

    private:
        RigModel& model;
        PatchBar patchBar;
        ScenePads scenePads;
        LiveStatusBar statusBar;
        int barHeight = 0;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LiveView)
    };
}
