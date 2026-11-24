#pragma once

#include "engine/AudioEngine.h"
#include "model/RigModel.h"
#include "ui/edit/EditView.h"
#include "ui/live/LiveView.h"
#include "ui/SpectrumWindow.h"   // [SPECTRUM]
#include "ui/StatusBar.h"
#include "ui/TopBar.h"
#include "tuner/TunerDialog.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace rig
{
    // 화면 배치
    //  ┌──────────────────────────────────────────────┐
    //  │ TopBar   : Patch [콤보] [◀][▶]  [패치명]  [Settings] │
    //  ├──────────────────────────────────────────────┤
    //  │ 중앙   : EditView or LiveView                 │
    //  ├──────────────────────────────────────────────┤
    //  │ StatusBar (62): [노브] IN | CPU | [MUTE] OUT [노브] │
    //  └──────────────────────────────────────────────┘
    //  중앙 = 창에서 TopBar(40) 와 StatusBar(62) 를 뗀 나머지
    //   - Edit 모드: EditView = 패치 맵 : 파라미터 바 : 블록 선택기 : 씬 바 = 3 : 1 : 3 : 1 (EditView.h 참고)
    //   - Live 모드: LiveView = 씬 패드 4 x 2 (나머지) + 패치 줄 [PREV][NEXT][Live] (1/8) (LiveView.h 참고)
    //   - 두 뷰는 같은 영역을 받고 setVisible 로 하나만 보인다 (파괴하지 않으므로 편집 상태 유지)
    //   - 맨 아래 줄은 둘 다 가운데 높이의 1/8 이라, 모드를 바꿔도 Live 버튼이 같은 자리에 있다
    //  TopBar 와 StatusBar 는 두 모드에서 그대로
    class MainComponent final : public juce::Component,
                                public juce::FileDragAndDropTarget,
                                private juce::Timer,
                                private juce::ChangeListener
    {
    public:
        MainComponent (juce::AudioDeviceManager& deviceManager, AudioEngine& engine, RigModel& model);
        ~MainComponent() override;

        // 창 크기 (고정)
        static constexpr int minWidth  = 1024;
        static constexpr int minHeight = 768;
        
        void paint (juce::Graphics&) override;
        void resized() override;

        bool isInterestedInFileDrag (const juce::StringArray& files) override;
        void filesDropped (const juce::StringArray& files, int x, int y) override;

    private:
        enum class ViewMode { edit, live };

        void setViewMode (ViewMode);   // 가운데만 EditView ↔ LiveView 로 바꾼다 (두 뷰는 파괴하지 않는다)
        void timerCallback() override;
        void openSettings();
        void openSpectrum();   // [SPECTRUM]
        void updateMuteButton();   // 뮤트 상태(RigModel 소유)를 버튼 색으로 보여준다
        void updateUndoButtons();  // 되돌릴 것 / 다시 할 것이 있을 때만 TopBar 의 Undo / Redo 활성
        void changeListenerCallback (juce::ChangeBroadcaster*) override;   // 모델이 바뀌면 Undo / Redo 상태
        void openTuner();          // 튜너 모달. 열려 있는 동안 출력 무음 (AudioEngine::setTunerActive)

        juce::AudioDeviceManager& deviceManager;
        AudioEngine& engine;
        RigModel& model;

        TopBar topBar;
        EditView editView;
        LiveView liveView;
        StatusBar statusBar;   // 볼륨 노브와 MUTE 버튼도 여기 있다

        ViewMode viewMode = ViewMode::edit;   // 항상 Edit 로 시작, 저장하지 않는다

        std::unique_ptr<SpectrumWindow> spectrumWindow;   // [SPECTRUM] 비모달 창. 닫으면 숨기기만 하고 MainComponent 와 함께 지워진다

        juce::ThreadPool libraryImports { 1 };   // 외부 파일 복사만 순차 실행. 완료 알림은 메시지 스레드로

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
    };
}
