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
    struct LibraryImportResult;

    // 화면 배치
    //  ┌──────────────────────────────────────────────┐
    //  │ TopBar   : Patch [콤보] [◀][▶]  [패치명]  [Settings] │
    //  ├──────────────────────────────────────────────┤
    //  │ 중앙   : EditView or LiveView                 │
    //  ├──────────────────────────────────────────────┤
    //  │ StatusBar (62): [노브] IN | CPU | [MUTE] OUT [노브] │
    //  └──────────────────────────────────────────────┘
    //  StatusBar 는 두 모드에서 그대로. 그 위가 모드마다 바뀐다
    //   - Edit 모드: TopBar(40) + EditView = 패치 맵 : 파라미터 바 : 블록 선택기 : 씬 바 = 3 : 1 : 3 : 1 (EditView.h 참고)
    //   - Live 모드: LiveView = 패치 줄 + 씬 패드 4 x 2 + 아래 줄 [채널][튜너][Scene/Stomp][Live] (LiveView.h 참고)
    //   - 두 뷰는 파괴하지 않고 setVisible 로 하나만 보인다 (편집 상태 유지). Live 에서는 TopBar 도 숨긴다
    //   - Live 의 위·아래 줄 높이 = 편집 화면 씬 바 높이 (EditView 높이의 1/8) 라, 모드를 바꿔도 Live 버튼이 같은 자리에 있다
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

        // 외부 파일 드롭 (Edit 모드에서만): .nam / .wav 를 라이브러리로 가져오고 결과를 알린다 (LibraryImport.h)
        bool isInterestedInFileDrag (const juce::StringArray& files) override;
        void filesDropped (const juce::StringArray& files, int x, int y) override;

    private:
        enum class ViewMode { edit, live };

        void setViewMode (ViewMode);   // TopBar + EditView ↔ LiveView (두 뷰는 파괴하지 않는다)
        void timerCallback() override;
        void openSettings();
        void openSpectrum();   // [SPECTRUM]
        void updateMuteButton();   // 뮤트 상태(RigModel 소유)를 버튼 색으로 보여준다
        void updateUndoButtons();  // 되돌릴 것 / 다시 할 것이 있을 때만 TopBar 의 Undo / Redo 활성
        void changeListenerCallback (juce::ChangeBroadcaster*) override;   // 모델이 바뀌면 Undo / Redo 상태
        void toggleTuner();        // 튜너 창 열기 / 닫기 (모달 아님). 열려 있는 동안 출력 무음 (AudioEngine::setTunerActive)
        void closeTuner();
        void libraryImported (const LibraryImportResult&);   // 드롭 가져오기가 끝난 뒤 (메시지 스레드)

        juce::AudioDeviceManager& deviceManager;
        AudioEngine& engine;
        RigModel& model;

        TopBar topBar;
        EditView editView;
        LiveView liveView;
        StatusBar statusBar;   // 볼륨 노브와 MUTE 버튼도 여기 있다

        ViewMode viewMode = ViewMode::edit;   // 항상 Edit 로 시작, 저장하지 않는다

        std::unique_ptr<TunerWindow> tunerWindow;   // 닫으면 숨기만 하고, 다음에 열 때 새로 만들거나 MainComponent 와 함께 지워진다

        std::unique_ptr<SpectrumWindow> spectrumWindow;   // [SPECTRUM] 비모달 창. 닫으면 숨기기만 하고 MainComponent 와 함께 지워진다

        juce::ThreadPool libraryImports { 1 };   // 외부 파일 복사만 순차 실행. 완료 알림은 메시지 스레드로

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
    };
}
