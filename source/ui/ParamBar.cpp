#include "ParamBar.h"

namespace rig
{
    ParamBar::ParamBar (RigModel& m) : model (m)
    {
        powerButton.setClickingTogglesState (true);
        powerButton.onClick = [this] { model.setOn (model.getSelectedIndex(), powerButton.getToggleState()); };
        addAndMakeVisible (powerButton);

        model.addChangeListener (this);
        updateFromModel();
    }

    ParamBar::~ParamBar()
    {
        model.removeChangeListener (this);
    }

    void ParamBar::changeListenerCallback (juce::ChangeBroadcaster*)
    {
        updateFromModel();
    }

    void ParamBar::updateFromModel()
    {
        const auto* slot = model.getSelectedSlot();

        // 미지정 블록은 지정할 때까지 끌 수 없다
        powerButton.setEnabled (slot != nullptr && slot->type != BlockType::None);
        powerButton.setToggleState (slot == nullptr || slot->on, juce::dontSendNotification);
    }

    void ParamBar::PowerButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
    {
        auto bounds = getLocalBounds().toFloat().reduced (1.5f);
        const bool on = getToggleState();

        auto colour = on ? juce::Colours::limegreen : juce::Colours::grey.darker (0.3f);
        if (! isEnabled())
            colour = juce::Colours::grey.darker (0.8f);
        else if (down)
            colour = colour.darker (0.3f);
        else if (highlighted)
            colour = colour.brighter (0.3f);

        g.setColour (juce::Colours::black.withAlpha (on && isEnabled() ? 0.35f : 0.2f));
        g.fillRoundedRectangle (bounds, 6.0f);
        g.setColour (colour.withAlpha (0.6f));
        g.drawRoundedRectangle (bounds, 6.0f, 1.0f);

        // 전원 기호: 위가 열린 원 + 세로선
        const auto icon = bounds.reduced (bounds.getHeight() * 0.25f);
        const auto centre = icon.getCentre();
        const float radius = icon.getHeight() * 0.5f;
        const float gapAngle = juce::MathConstants<float>::pi / 5.0f;

        juce::Path path;
        path.addCentredArc (centre.x, centre.y, radius, radius, 0.0f,
                            gapAngle, juce::MathConstants<float>::twoPi - gapAngle, true);
        path.startNewSubPath (centre.x, icon.getY() - radius * 0.15f);
        path.lineTo (centre.x, centre.y);

        g.setColour (colour);
        g.strokePath (path, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void ParamBar::paint (juce::Graphics& g)
    {
        g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId).darker (0.05f));
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.drawHorizontalLine (0, 0.0f, (float) getWidth());
    }

    void ParamBar::resized()
    {
        // 오른쪽 끝 = 전원 버튼. 나머지(왼쪽)는 나중에 파라미터 노브 자리
        auto area = getLocalBounds().reduced (12, 0);
        const int size = juce::jmin (40, getHeight() - 16);
        powerButton.setBounds (area.removeFromRight (size).withSizeKeepingCentre (size, size));
    }
}
