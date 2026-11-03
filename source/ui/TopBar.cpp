#include "TopBar.h"

namespace rig
{
    TopBar::TopBar (int numPatches)
    {
        for (int i = 1; i <= numPatches; ++i)
            patchBox.addItem (juce::String (i).paddedLeft ('0', 2), i);

        patchBox.onChange = [this]
        {
            if (onPatchChanged)
                onPatchChanged (patchBox.getSelectedId());
        };

        settingsButton.onClick = [this]
        {
            if (onSettingsClicked)
                onSettingsClicked();
        };

        patchLabel.setJustificationType (juce::Justification::centredRight);

        addAndMakeVisible (patchLabel);
        addAndMakeVisible (patchBox);
        addAndMakeVisible (settingsButton);
    }

    void TopBar::setPatchNumber (int number)
    {
        patchBox.setSelectedId (number, juce::dontSendNotification);
    }

    void TopBar::paint (juce::Graphics& g)
    {
        g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId).darker (0.3f));
    }

    void TopBar::resized()
    {
        auto area = getLocalBounds().reduced (8, 6);
        patchLabel.setBounds (area.removeFromLeft (50));
        area.removeFromLeft (4);
        patchBox.setBounds (area.removeFromLeft (80));
        settingsButton.setBounds (area.removeFromRight (110));
    }
}
