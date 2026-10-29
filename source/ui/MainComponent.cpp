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
        topBar.onPatchChanged = [this] (int number) { model.loadPatch (number); };
        topBar.onSettingsClicked = [this] { openSettings(); };

        addAndMakeVisible (topBar);
        addAndMakeVisible (chainView);
        addAndMakeVisible (blockEditor);
        addAndMakeVisible (statusBar);

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

    void MainComponent::timerCallback()
    {
        statusBar.update (engine.takeInputPeak(), deviceManager.getCpuUsage(), engine.takeOutputPeak());
        engine.collectGarbage();
    }

    void MainComponent::paint (juce::Graphics& g)
    {
        g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

        g.setColour (juce::Colours::grey.withAlpha (0.5f));
        g.setFont (juce::FontOptions (13.0f));
        g.drawText ("(reserved for future features)", reservedArea, juce::Justification::centred);
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
    }
}
