#include "TopBar.h"

namespace rig
{
    TopBar::TopBar (int numPatches)
    {
        for (int i = 1; i <= numPatches; ++i)
            patchBox.addItem (juce::String (i).paddedLeft ('0', 2), i);

        const auto arrowColour = previousPatchButton.findColour (juce::TextButton::textColourOffId);

        juce::DrawablePath previousPatchIcon;
        juce::Path previousPatchPath;
        previousPatchPath.addTriangle (10.0f, 0.0f, 10.0f, 10.0f, 0.0f, 5.0f);
        previousPatchIcon.setPath (previousPatchPath);
        previousPatchIcon.setFill (arrowColour);
        previousPatchButton.setImages (&previousPatchIcon);

        juce::DrawablePath nextPatchIcon;
        juce::Path nextPatchPath;
        nextPatchPath.addTriangle (0.0f, 0.0f, 0.0f, 10.0f, 10.0f, 5.0f);
        nextPatchIcon.setPath (nextPatchPath);
        nextPatchIcon.setFill (arrowColour);
        nextPatchButton.setImages (&nextPatchIcon);

        patchBox.onChange = [this]
        {
            updateNavigationButtons();

            if (onPatchChanged)
                onPatchChanged (patchBox.getSelectedId());
        };

        previousPatchButton.onClick = [this] { selectAdjacentPatch (-1); };
        nextPatchButton.onClick = [this] { selectAdjacentPatch (1); };

        previousPatchButton.setTooltip ("Previous patch");
        nextPatchButton.setTooltip ("Next patch");

        settingsButton.onClick = [this]
        {
            if (onSettingsClicked)
                onSettingsClicked();
        };

        patchLabel.setJustificationType (juce::Justification::centredRight);

        addAndMakeVisible (patchLabel);
        addAndMakeVisible (patchBox);
        addAndMakeVisible (previousPatchButton);
        addAndMakeVisible (nextPatchButton);
        addAndMakeVisible (settingsButton);

        updateNavigationButtons();
    }

    void TopBar::setPatchNumber (int number)
    {
        patchBox.setSelectedId (number, juce::dontSendNotification);
        updateNavigationButtons();
    }

    void TopBar::selectAdjacentPatch (int delta)
    {
        const auto nextPatch = patchBox.getSelectedId() + delta;

        if (juce::isPositiveAndNotGreaterThan (nextPatch, patchBox.getNumItems()))
            patchBox.setSelectedId (nextPatch, juce::sendNotificationSync);
    }

    void TopBar::updateNavigationButtons()
    {
        const auto patchNumber = patchBox.getSelectedId();
        previousPatchButton.setEnabled (patchNumber > 1);
        nextPatchButton.setEnabled (patchNumber > 0 && patchNumber < patchBox.getNumItems());
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
        area.removeFromLeft (4);
        previousPatchButton.setBounds (area.removeFromLeft (28));
        area.removeFromLeft (4);
        nextPatchButton.setBounds (area.removeFromLeft (28));
        settingsButton.setBounds (area.removeFromRight (110));
    }
}
