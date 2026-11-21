#include "MainComponent.h"

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

        statusBar.inputKnob.onValueChanged  = [this] (float db) { model.setInputGainDb (db); };
        statusBar.outputKnob.onValueChanged = [this] (float db)
        {
            model.setOutputGainDb (db);   // 뮤트도 함께 해제됨
            updateMuteButton();
        };

        // 볼륨 저장은 드래그가 끝났을 때 한 번만
        statusBar.inputKnob.onDragEnded  = [this] { model.saveGains(); };
        statusBar.outputKnob.onDragEnded = [this] { model.saveGains(); };

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
        model.removeChangeListener (this);
    }

    void MainComponent::changeListenerCallback (juce::ChangeBroadcaster*)
    {
        updateUndoButtons();
    }

    void MainComponent::updateUndoButtons()
    {
        topBar.setUndoRedoEnabled (model.canUndo(), model.canRedo());
    }

    void MainComponent::openSettings()
    {
        juce::Component::SafePointer<MainComponent> safeThis (this);

        SettingsDialog::show (deviceManager, this, [safeThis] (bool saved)
        {
            if (safeThis != nullptr && saved)
                safeThis->model.deviceChanged();
        });
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
}
