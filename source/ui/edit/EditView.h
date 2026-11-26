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
    //  │ SceneBar    : ssbar [1]…[8] (1/8)              │
    //  └──────────────────────────────────────────────┘
    // 끌어 놓기의 컨테이너: 선택기 → 패치맵, 패치맵 안 블록 이동, ssbar 칸끼리, 패치맵 블록 → ssbar Stomp 칸 (Stomp 모드).
    // 끌기 시작·끝을 ssbar 에 알려 놓을 수 있는 칸을 보이게 하고, 패치맵 블록이 선택기 높이 아래로 오면 ssbar 로 가는 것으로 본다
    class EditView final : public juce::Component,
                           public juce::DragAndDropContainer
    {
    public:
        EditView (RigModel& model, juce::AudioDeviceManager& deviceManager);

        std::function<void()> onLiveClicked;   // 씬 바의 Live 버튼

        void refreshLibrary() { blockEditor.refresh(); }                                          // 설정창 Refresh Library
        void addLibraryFiles (const juce::Array<juce::File>& files) { blockEditor.addFiles (files); }   // 드롭 가져오기

        void showSpectrumButton (std::function<void()> onClick) { chainView.showSpectrumButton (std::move (onClick)); }   // [SPECTRUM]

        void cancelEditing() { sceneBar.cancelEditing(); }   // 씬 바의 씬 이름 편집을 끝낸다

        void resized() override;
        void visibilityChanged() override;   // 숨겨진 동안(Live 모드) 무거운 갱신을 멈추고, 다시 보이면 한 번 맞춘다

    private:
        void dragOperationStarted (const juce::DragAndDropTarget::SourceDetails&) override;
        void dragOperationEnded (const juce::DragAndDropTarget::SourceDetails&) override;

        ChainView chainView;
        ParamBar paramBar;
        BlockEditor blockEditor;
        SceneBar sceneBar;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EditView)
    };
}
