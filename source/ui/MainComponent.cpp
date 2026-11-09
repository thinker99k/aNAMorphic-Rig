#include "MainComponent.h"

#include "ui/SettingsDialog.h"

namespace rig
{
    MainComponent::MainComponent (juce::AudioDeviceManager& dm, AudioEngine& e, RigModel& m)
        : deviceManager (dm),
          engine (e),
          model (m),
          topBar (RigModel::numPatches),
          chainView (m),
          blockEditor (m, e)
    {
        topBar.setPatchNumber (model.getPatchNumber());
        topBar.setPatchName (model.getPatchName());
        topBar.onPatchNameChanged = [this] (const juce::String& name)
        {
            model.setPatchName (name);
            topBar.setPatchName (model.getPatchName());
        };
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

        addAndMakeVisible (topBar);
        addAndMakeVisible (chainView);
        addAndMakeVisible (blockEditor);
        addAndMakeVisible (statusBar);

        statusBar.inputKnob.setValueDb (model.getInputGainDb());
        statusBar.outputKnob.setValueDb (model.getOutputGainDb());
        updateMuteButton();

        startTimerHz (30);
        setSize (minWidth, minHeight);
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
        constexpr int statusBarHeight = 68;   // 노브(56) 위아래로 6 px 씩 여유

        auto area = getLocalBounds();

        topBar.setBounds (area.removeFromTop (40));
        statusBar.setBounds (area.removeFromBottom (statusBarHeight));

        const int half = area.getHeight() / 2;
        chainView.setBounds (area.removeFromTop (half));
        blockEditor.setBounds (area.removeFromTop (area.getHeight() / 2));
        reservedArea = area;
    }
}
