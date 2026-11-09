#include "TopBar.h"

namespace rig
{
    TopBar::TopBar (int numPatches)
    {
        for (int i = 1; i <= numPatches; ++i)
            patchBox.addItem (juce::String (i).paddedLeft ('0', 2), i);

        patchBox.onChange = [this]
        {
            patchNameBox.hideEditor (false);
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
        addAndMakeVisible (patchNameBox);
        addAndMakeVisible (editNameButton);
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

        const int sideMargin = juce::jmax (area.getX(), getWidth() - area.getRight()) + 48;
        const int nameWidth = juce::jlimit (0, 360, getWidth() - 2 * sideMargin);
        patchNameBox.setBounds ((getWidth() - nameWidth) / 2, area.getY(), nameWidth, area.getHeight());
        editNameButton.setBounds (patchNameBox.getRight() + 6, area.getY(), 28, area.getHeight());
    }
}
