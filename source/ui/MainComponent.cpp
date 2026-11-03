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
        topBar.onPatchChanged = [this] (int number)
        {
            model.loadPatch (number);

            // 알림 없이 값만 바꾼다 (알림이 가면 새 패치 파일을 다시 저장하게 됨)
            // 뮤트는 패치를 바꿔도 유지되므로, 뮤트 중이면 출력 노브는 최솟값에 둔다
            inputKnob.setValueDb (model.getInputGainDb());
            outputKnob.setValueDb (model.isOutputMuted() ? AudioEngine::outputMinDb
                                                         : model.getOutputGainDb());
            updateMuteButton();
        };
        topBar.onSettingsClicked = [this] { openSettings(); };

        inputKnob.setValueDb (model.getInputGainDb());
        outputKnob.setValueDb (model.getOutputGainDb());
        inputKnob.onValueChanged  = [this] (float db) { model.setInputGainDb (db); };
        outputKnob.onValueChanged = [this] (float db)
        {
            model.setOutputGainDb (db);   // 뮤트도 함께 해제됨
            updateMuteButton();
        };

        muteButton.onClick = [this]
        {
            model.setOutputMuted (! model.isOutputMuted());
            outputKnob.setValueDb (model.isOutputMuted() ? AudioEngine::outputMinDb
                                                         : model.getOutputGainDb());
            updateMuteButton();
        };

        addAndMakeVisible (topBar);
        addAndMakeVisible (chainView);
        addAndMakeVisible (blockEditor);
        addAndMakeVisible (statusBar);
        addAndMakeVisible (inputKnob);
        addAndMakeVisible (outputKnob);
        addAndMakeVisible (muteButton);
        updateMuteButton();

        startTimerHz (30);
        setSize (1024, 720);
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
        auto area = getLocalBounds();

        topBar.setBounds (area.removeFromTop (40));
        statusBar.setBounds (area.removeFromBottom (30));

        const int half = area.getHeight() / 2;
        chainView.setBounds (area.removeFromTop (half));
        blockEditor.setBounds (area.removeFromTop (area.getHeight() / 2));
        reservedArea = area;

        // INPUT 은 맨 왼쪽, MUTE 는 맨 오른쪽 끝, OUTPUT 은 MUTE 바로 왼쪽
        constexpr int buttonWidth = 60, buttonHeight = 28, gap = 8;

        auto knobArea = reservedArea.reduced (8, 4);
        auto muteArea = knobArea.removeFromRight (buttonWidth);
        muteButton.setBounds (muteArea.withSizeKeepingCentre (buttonWidth, buttonHeight));
        knobArea.removeFromRight (gap);

        const int knobWidth = juce::jmin (100, knobArea.getWidth() / 2);
        outputKnob.setBounds (knobArea.removeFromRight (knobWidth));
        inputKnob.setBounds (knobArea.removeFromLeft (knobWidth));
    }
}
