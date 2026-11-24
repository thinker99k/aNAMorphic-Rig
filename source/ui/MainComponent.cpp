#include "MainComponent.h"

#include "AppPaths.h"
#include "library/LibraryImport.h"
#include "ui/SettingsDialog.h"

namespace rig
{
    MainComponent::MainComponent (juce::AudioDeviceManager& dm, AudioEngine& e, RigModel& m)
        : deviceManager (dm),
          engine (e),
          model (m),
          topBar (RigModel::numPatches),
          editView (m, dm),
          liveView (m)
    {
        topBar.setPatchNumber (model.getPatchNumber());
        topBar.setPatchName (model.getPatchName());
        topBar.onPatchNameChanged = [this] (const juce::String& name)
        {
            model.setPatchName (name);
            topBar.setPatchName (model.getPatchName());
            updateUndoButtons();   // 이름 바꾸기는 변경 알림을 보내지 않는다
        };

        // 되돌리면 패치 이름도 돌아올 수 있다 (이름은 TopBar 가 들고 있으므로 다시 넣는다)
        topBar.onUndo = [this] { model.undo(); topBar.setPatchName (model.getPatchName()); updateUndoButtons(); };
        topBar.onRedo = [this] { model.redo(); topBar.setPatchName (model.getPatchName()); updateUndoButtons(); };
        topBar.onPatchChanged = [this] (int number)
        {
            model.loadPatch (number);
            topBar.setPatchName (model.getPatchName());

            // 알림 없이 값만 바꾼다 (알림이 가면 새 패치 파일을 다시 저장하게 됨)
            // 뮤트는 패치를 바꿔도 유지되므로, 뮤트 중이면 출력 노브는 최솟값에 둔다
            statusBar.inputKnob.setValueDb (model.getInputGainDb());
            statusBar.outputKnob.setValueDb (model.isOutputMuted() ? AudioEngine::outputMinDb
                                                                   : model.getOutputGainDb());
            updateMuteButton();
        };
        topBar.onPatchReset = [this]
        {
            model.resetPatch();
            topBar.setPatchName (model.getPatchName());
        };
        topBar.onSettingsClicked = [this] { openSettings(); };
        topBar.onTunerClicked = [this] { openTuner(); };

        // [SPECTRUM] begin (테스트용) 실행 인자 --spectrum 을 줬을 때만 패치맵 오른쪽 위에 버튼
        if (juce::JUCEApplicationBase::getCommandLineParameterArray().contains ("--spectrum"))
            editView.showSpectrumButton ([this] { openSpectrum(); });
        // [SPECTRUM] end

        statusBar.inputKnob.onValueChanged  = [this] (float db) { model.setInputGainDb (db); };
        statusBar.outputKnob.onValueChanged = [this] (float db)
        {
            model.setOutputGainDb (db);   // 뮤트도 함께 해제됨
            updateMuteButton();
        };

        model.onPatchSaved = [this] (bool ok, const juce::File& file) { reportWriteResult (*this, ok, file); };

        // 볼륨 저장은 드래그가 끝났을 때 한 번만
        statusBar.inputKnob.onDragEnded  = [this] { model.saveAppSettings(); };
        statusBar.outputKnob.onDragEnded = [this] { model.saveAppSettings(); };

        statusBar.muteButton.onClick = [this]
        {
            model.setOutputMuted (! model.isOutputMuted());
            statusBar.outputKnob.setValueDb (model.isOutputMuted() ? AudioEngine::outputMinDb
                                                                   : model.getOutputGainDb());
            updateMuteButton();
        };

        // 가운데 화면 전환: Live 의 PREV / NEXT 는 TopBar ◀▶ 와 같은 경로 (onPatchChanged)
        editView.onLiveClicked = [this] { setViewMode (ViewMode::live); };
        liveView.onLiveOff     = [this] { setViewMode (ViewMode::edit); };
        liveView.onPatchStep   = [this] (int delta) { topBar.selectAdjacentPatch (delta); };

        addAndMakeVisible (topBar);
        addAndMakeVisible (editView);
        addAndMakeVisible (liveView);
        addAndMakeVisible (statusBar);
        liveView.setVisible (false);   // 항상 Edit 로 시작

        statusBar.inputKnob.setValueDb (model.getInputGainDb());
        statusBar.outputKnob.setValueDb (model.getOutputGainDb());
        updateMuteButton();

        model.addChangeListener (this);
        updateUndoButtons();

        startTimerHz (30);
        setSize (minWidth, minHeight);
    }

    MainComponent::~MainComponent()
    {
        libraryImports.removeAllJobs (true, 10000);   // 대기 작업 취소, 진행 중 복사는 버퍼 경계에서 정리 (RigModel 로더와 같게 최대 10초)
        model.removeChangeListener (this);
        model.onPatchSaved = nullptr;
    }

    void MainComponent::changeListenerCallback (juce::ChangeBroadcaster*)
    {
        updateUndoButtons();
    }

    void MainComponent::updateUndoButtons()
    {
        topBar.setUndoRedoEnabled (model.canUndo(), model.canRedo());
    }

    // [SPECTRUM] begin (테스트용)
    void MainComponent::openSpectrum()
    {
        if (spectrumWindow == nullptr)
        {
            spectrumWindow = std::make_unique<SpectrumWindow> (engine);

            // 처음 열 때: 메인 창 바로 아래에 들어가면 거기, 아니면 메인 창 가운데
            const auto mainBounds = getTopLevelComponent()->getScreenBounds();
            const juce::Rectangle<int> below (mainBounds.getX(), mainBounds.getBottom() + 8,
                                              spectrumWindow->getWidth(), spectrumWindow->getHeight());
            const auto* display = juce::Desktop::getInstance().getDisplays().getDisplayForRect (mainBounds);

            if (display != nullptr && display->userBounds.contains (below.toFloat()))
                spectrumWindow->setBounds (below);
            else
                spectrumWindow->centreAroundComponent (this, spectrumWindow->getWidth(), spectrumWindow->getHeight());
        }

        // 이미 있으면(숨겨져 있든 보이든) 새로 만들지 않고 앞으로 가져온다
        spectrumWindow->setVisible (true);
        spectrumWindow->toFront (true);
    }
    // [SPECTRUM] end

    void MainComponent::openSettings()
    {
        juce::Component::SafePointer<MainComponent> safeThis (this);

        SettingsDialog::show (deviceManager, this, [safeThis] (bool saved)
        {
            if (safeThis != nullptr && saved)
                safeThis->model.deviceChanged();
        }, [safeThis]
        {
            if (safeThis != nullptr)
                safeThis->editView.refreshLibrary();
        });
    }

    // Edit 모드에서만 받는다. 받은 항목은 모두 처리하고 결과를 알린다 (폴더, 다른 파일도 실패로 보여 준다)
    bool MainComponent::isInterestedInFileDrag (const juce::StringArray&)
    {
        return viewMode == ViewMode::edit;
    }

    void MainComponent::filesDropped (const juce::StringArray& files, int x, int y)
    {
        // MainComponent 만 파일 드롭 대상이다. 제목 표시줄은 부모 DocumentWindow 영역.
        if (viewMode != ViewMode::edit || ! getLocalBounds().contains (x, y) || isCurrentlyBlockedByAnotherModalComponent())
            return;

        const auto namDirectory = paths::namDir();
        const auto irDirectory = paths::irDir();
        juce::Component::SafePointer<MainComponent> safeThis (this);

        libraryImports.addJob ([files, namDirectory, irDirectory, safeThis]
        {
            auto* job = juce::ThreadPoolJob::getCurrentThreadPoolJob();
            auto imported = importLibraryFiles (files, namDirectory, irDirectory, [job] { return job->shouldExit(); });
            if (imported.cancelled || job->shouldExit())
                return;

            // SafePointer 는 이 UI 콜백에서만 검사한다 (작업 스레드는 UI 에 접근하지 않는다).
            juce::MessageManager::callAsync ([safeThis, result = std::move (imported)]
            {
                if (safeThis != nullptr)
                    safeThis->libraryImported (result);
            });
        });
    }

    // 드롭 한 번에 결과 알림 하나: 성공 x / 실패 y, 실패가 있으면 그 이름 목록
    void MainComponent::libraryImported (const LibraryImportResult& result)
    {
        if (! result.imported.isEmpty())
            editView.refreshLibrary();

        const int succeeded = result.imported.size(), failed = result.failed.size();
        juce::StringArray lines;

        if (succeeded > 0)
            lines.add (juce::String (succeeded) + " imported");
        if (failed > 0)
            lines.add (juce::String (failed) + " failed");

        showNotice (*this, "Library Import", lines.joinIntoString ("\n"), result.failed);
    }

    void MainComponent::updateMuteButton()
    {
        // 토글 상태는 쓰지 않는다: 상태의 주인은 RigModel, 버튼은 색으로 보여주기만 한다
        const bool muted = model.isOutputMuted();
        auto& muteButton = statusBar.muteButton;
        muteButton.setColour (juce::TextButton::buttonColourId,
                              muted ? juce::Colours::red.darker (0.2f) : juce::Colours::darkgrey);
        muteButton.setColour (juce::TextButton::textColourOffId,
                              muted ? juce::Colours::white : juce::Colours::grey);
        muteButton.repaint();
    }

    void MainComponent::timerCallback()
    {
        statusBar.update (engine.takeInputPeak(), deviceManager.getCpuUsage(), engine.takeOutputPeak());
        engine.collectGarbage();
    }

    void MainComponent::paint (juce::Graphics& g)
    {
        g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));


    }

    void MainComponent::resized()
    {
        constexpr int statusBarHeight = 62;   // 노브 호 위 여백 9 px, 몸통 아래 여백 7 px (StatusBar.cpp topExtra 참고)

        auto area = getLocalBounds();

        topBar.setBounds (area.removeFromTop (40));
        statusBar.setBounds (area.removeFromBottom (statusBarHeight));

        editView.setBounds (area);
        liveView.setBounds (area);   // 보이는 쪽만 그려진다
    }

    void MainComponent::setViewMode (ViewMode mode)
    {
        if (mode == viewMode)
            return;

        viewMode = mode;
        const bool live = (mode == ViewMode::live);

        if (live)
            editView.cancelEditing();   // 열려 있던 Copy/Swap 모드, 이름 편집을 끝낸다

        editView.setVisible (! live);
        liveView.setVisible (live);
    }

    void MainComponent::openTuner()
    {
        // 튜너가 열려 있는 동안 출력은 엔진이 무음으로 한다 (MUTE 버튼·출력 노브는 그대로)
        topBar.setTunerActive (true);

        juce::Component::SafePointer<MainComponent> safeThis (this);

        TunerDialog::show (engine, model, this, [safeThis]
        {
            if (safeThis != nullptr)
                safeThis->topBar.setTunerActive (false);
        });
    }
}
