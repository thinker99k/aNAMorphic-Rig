#include "ParamSwitch.h"

namespace rig
{
    ParamSwitch::ParamSwitch (const juce::String& text) : LeftClickOnly (text)
    {
        setClickingTogglesState (true);
    }

    void ParamSwitch::paintButton (juce::Graphics& g, bool highlighted, bool down)
    {
        auto bounds = getLocalBounds().toFloat();
        const bool on = getToggleState();

        auto colour = on ? juce::Colours::limegreen : juce::Colours::grey.darker (0.3f);
        if (! isEnabled())
            colour = juce::Colours::grey.darker (0.8f);
        else if (down)
            colour = colour.darker (0.3f);
        else if (highlighted)
            colour = colour.brighter (0.3f);

        // 알약: 높이의 일부, 폭은 그 2 배. 오른쪽에 붙인다
        const float pillHeight = juce::jmin (bounds.getHeight() * 0.6f, bounds.getWidth() * 0.25f);
        const auto pill = bounds.removeFromRight (pillHeight * 2.0f).withSizeKeepingCentre (pillHeight * 2.0f, pillHeight);

        g.setColour (on ? colour.withAlpha (0.85f) : colour.withAlpha (0.35f));
        g.fillRoundedRectangle (pill, pillHeight * 0.5f);
        g.setColour (colour);
        g.drawRoundedRectangle (pill, pillHeight * 0.5f, 1.0f);

        const float thumb = pillHeight * 0.7f;
        const float thumbX = on ? pill.getRight() - pillHeight * 0.15f - thumb : pill.getX() + pillHeight * 0.15f;
        g.setColour (on ? juce::Colours::white : juce::Colours::lightgrey.withAlpha (0.7f));
        g.fillEllipse (thumbX, pill.getCentreY() - thumb * 0.5f, thumb, thumb);

        // 글자: 알약 왼쪽
        g.setColour (juce::Colours::lightgrey);
        g.setFont (juce::FontOptions (pillHeight * 0.9f, juce::Font::bold));
        g.drawText (getButtonText(), bounds.withTrimmedRight (pillHeight * 0.4f), juce::Justification::centredRight);
    }
}
