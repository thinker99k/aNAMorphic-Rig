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
        patchNameBox.setJustificationType (juce::Justification::centred);
        patchNameBox.setColour (juce::Label::backgroundColourId, juce::Colours::black.withAlpha (0.25f));
        patchNameBox.setColour (juce::Label::outlineColourId, juce::Colours::grey);

        addAndMakeVisible (patchLabel);
        addAndMakeVisible (patchBox);
        addAndMakeVisible (patchNameBox);
        addAndMakeVisible (settingsButton);
    }

    void TopBar::setPatchNumber (int number)
    {
        patchBox.setSelectedId (number, juce::dontSendNotification);
    }

    void TopBar::setPatchName (const juce::String& name)
    {
        patchNameBox.setText (name, juce::dontSendNotification);
        patchNameBox.setTooltip (name);
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

        const int sideMargin = juce::jmax (area.getX(), getWidth() - area.getRight()) + 16;
        const int nameWidth = juce::jlimit (0, 360, getWidth() - 2 * sideMargin);
        patchNameBox.setBounds ((getWidth() - nameWidth) / 2, area.getY(), nameWidth, area.getHeight());
    }
}
