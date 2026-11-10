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
            patchNameBox.hideEditor (false);
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
        patchNameBox.setJustificationType (juce::Justification::centred);
        patchNameBox.setColour (juce::Label::backgroundColourId, juce::Colours::black.withAlpha (0.25f));
        patchNameBox.setColour (juce::Label::outlineColourId, juce::Colours::grey);
        patchNameBox.setEditable (false, false, false);
        patchNameBox.onTextChange = [this]
        {
            if (onPatchNameChanged)
                onPatchNameChanged (patchNameBox.getText());
        };

        juce::Path pencil;
        pencil.startNewSubPath (3.0f, 14.0f);
        pencil.lineTo (13.0f, 4.0f);
        pencil.lineTo (17.0f, 8.0f);
        pencil.lineTo (7.0f, 18.0f);
        pencil.lineTo (2.0f, 19.0f);
        pencil.closeSubPath();
        pencil.startNewSubPath (14.0f, 3.0f);
        pencil.lineTo (16.0f, 1.0f);
        pencil.lineTo (20.0f, 5.0f);
        pencil.lineTo (18.0f, 7.0f);
        pencil.closeSubPath();
        juce::DrawablePath pencilImage;
        pencilImage.setPath (pencil);
        pencilImage.setFill (juce::Colours::lightgrey);
        editNameButton.setImages (&pencilImage);
        editNameButton.setEdgeIndent (6);
        editNameButton.setTooltip ("Edit patch name (Enter to save, Escape to cancel)");
        editNameButton.onClick = [this] { patchNameBox.showEditor(); };

        addAndMakeVisible (patchLabel);
        addAndMakeVisible (patchBox);
        addAndMakeVisible (previousPatchButton);
        addAndMakeVisible (nextPatchButton);
        addAndMakeVisible (patchNameBox);
        addAndMakeVisible (editNameButton);
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

        if (nextPatch >= 1 && nextPatch <= patchBox.getNumItems())
            patchBox.setSelectedId (nextPatch, juce::sendNotificationSync);
    }

    void TopBar::updateNavigationButtons()
    {
        const auto patchNumber = patchBox.getSelectedId();
        previousPatchButton.setEnabled (patchNumber > 1);
        nextPatchButton.setEnabled (patchNumber > 0 && patchNumber < patchBox.getNumItems());
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
        area.removeFromLeft (4);
        previousPatchButton.setBounds (area.removeFromLeft (28));
        area.removeFromLeft (4);
        nextPatchButton.setBounds (area.removeFromLeft (28));
        settingsButton.setBounds (area.removeFromRight (110));

        const int sideMargin = juce::jmax (area.getX(), getWidth() - area.getRight()) + 48;
        const int nameWidth = juce::jlimit (0, 360, getWidth() - 2 * sideMargin);
        patchNameBox.setBounds ((getWidth() - nameWidth) / 2, area.getY(), nameWidth, area.getHeight());
        editNameButton.setBounds (patchNameBox.getRight() + 6, area.getY(), 28, area.getHeight());
    }
}
