#pragma once

#include "model/RigModel.h"
#include "ui/edit/BlockEditor.h"
#include "ui/edit/ChainView.h"
#include "ui/edit/ParamBar.h"
#include "ui/edit/SceneBar.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace rig
{
    // 편집 화면 (TopBar 와 StatusBar 사이)
    //  ┌──────────────────────────────────────────────┐
    //  │ ChainView   : 패치 맵 (3/8)                   │
    //  ├──────────────────────────────────────────────┤
    //  │ ParamBar    : 선택 블록 파라미터 (1/8)         │
    //  ├──────────────────────────────────────────────┤
    //  │ BlockEditor : 블록 선택기 (3/8)               │
    //  ├──────────────────────────────────────────────┤
    //  │ SceneBar    : [1]…[8] [Copy/Swap] (1/8)        │
    //  └──────────────────────────────────────────────┘
    class EditView final : public juce::Component
    {
    public:
        EditView (RigModel& model, juce::AudioDeviceManager& deviceManager);

        std::function<void()> onLiveClicked;   // 씬 바의 Live 버튼

        void cancelEditing() { sceneBar.cancelEditing(); }   // 씬 바의 Copy / Swap 모드와 씬 이름 편집을 끝낸다

        void resized() override;

    private:
        ChainView chainView;
        ParamBar paramBar;
        BlockEditor blockEditor;
        SceneBar sceneBar;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EditView)
    };
}
